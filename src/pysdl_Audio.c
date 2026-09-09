#include "pysdl.h"

// SDL3 audio: a device is driven through an SDL_AudioStream. Audio.Open() wraps
// SDL_OpenAudioDeviceStream; with a callback the trampolines below push (playback)
// or drain (recording) the bound stream. Without a callback, feed the stream
// directly with Queue() / drain it with Dequeue().

static int        PySDL_Audio_Type_init    (PySDL_Audio*, PyObject*, PyObject*);
static void       PySDL_Audio_Type_dealloc (PySDL_Audio* );

static PyObject * PySDL_Audio_Open         (PySDL_Audio*, PyObject*, PyObject*);
static PyObject * PySDL_Audio_Close        (PySDL_Audio*, PyObject*);
static PyObject * PySDL_Audio_Pause        (PySDL_Audio*, PyObject*);
static PyObject * PySDL_Audio_Lock         (PySDL_Audio*, PyObject*);
static PyObject * PySDL_Audio_Unlock       (PySDL_Audio*, PyObject*);
static PyObject * PySDL_Audio_Queue        (PySDL_Audio*, PyObject*);
static PyObject * PySDL_Audio_Dequeue      (PySDL_Audio*, PyObject*);
static PyObject * PySDL_Audio_GetQueueSize (PySDL_Audio*, PyObject*);
static PyObject * PySDL_Audio_ClearQueued  (PySDL_Audio*, PyObject*);
static PyObject * PySDL_Audio_GetStatus    (PySDL_Audio*, PyObject*);

static PyMethodDef PySDL_Audio_methods[] = {
    { "Open",         (PyCFunction)PySDL_Audio_Open,         METH_VARARGS | METH_KEYWORDS },
    { "Close",        (PyCFunction)PySDL_Audio_Close,        METH_NOARGS  },
    { "Pause",        (PyCFunction)PySDL_Audio_Pause,        METH_O       },
    { "Lock",         (PyCFunction)PySDL_Audio_Lock,         METH_NOARGS  },
    { "Unlock",       (PyCFunction)PySDL_Audio_Unlock,       METH_NOARGS  },
    { "Queue",        (PyCFunction)PySDL_Audio_Queue,        METH_O       },
    { "Dequeue",      (PyCFunction)PySDL_Audio_Dequeue,      METH_O       },
    { "GetQueueSize", (PyCFunction)PySDL_Audio_GetQueueSize, METH_NOARGS  },
    { "ClearQueued",  (PyCFunction)PySDL_Audio_ClearQueued,  METH_NOARGS  },
    { "GetStatus",    (PyCFunction)PySDL_Audio_GetStatus,    METH_NOARGS  },
    { NULL }
};

PyTypeObject PySDL_Audio_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Audio",
    .tp_basicsize = sizeof(PySDL_Audio),
    .tp_dealloc   = (destructor)PySDL_Audio_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Audio - device-bound SDL_AudioStream",
    .tp_methods   = PySDL_Audio_methods,
    .tp_init      = (initproc)PySDL_Audio_Type_init,
    .tp_new       = PyType_GenericNew
};

// (callback, userdata) tuple is passed as the stream's userdata.
static void SDLCALL _playback_callback(void *data, SDL_AudioStream *stream, int additional, int total) {
    (void)total;
    if(additional <= 0) {
        return;
    }
    PyGILState_STATE gil;
    if(!PySDL_ThreadEnter(&gil)) {
        return;
    }

    PyObject *callback = PyTuple_GET_ITEM((PyObject *)data, 0);
    PyObject *userdata = PyTuple_GET_ITEM((PyObject *)data, 1);
    PyObject *size     = PyLong_FromLong(additional);

    PyObject *audioData = PyObject_CallFunctionObjArgs(callback, size, userdata, NULL);
    if(NULL == audioData) {
        PyErr_Print();
    } else if(!PyBytes_Check(audioData)) {
        PyErr_Format(PyExc_TypeError, "audio callback must return bytes, not %s", Py_TYPE(audioData)->tp_name);
        PyErr_Print();
        Py_DECREF(audioData);
    } else {
        SDL_PutAudioStreamData(stream, PyBytes_AS_STRING(audioData), (int)PyBytes_GET_SIZE(audioData));
        Py_DECREF(audioData);
    }

    Py_XDECREF(size);
    PySDL_ThreadLeave(gil);
}

static void SDLCALL _capture_callback(void *data, SDL_AudioStream *stream, int additional, int total) {
    (void)additional;
    (void)total;
    int avail = SDL_GetAudioStreamAvailable(stream);
    if(avail <= 0) {
        return;
    }
    PyGILState_STATE gil;
    if(!PySDL_ThreadEnter(&gil)) {
        return;
    }

    PyObject *buffer = PyBytes_FromStringAndSize(NULL, avail);
    if(NULL != buffer) {
        int got = SDL_GetAudioStreamData(stream, PyBytes_AS_STRING(buffer), avail);
        if(got > 0) {
            if(got != avail) {
                _PyBytes_Resize(&buffer, got);
            }
            if(NULL != buffer) {
                PyObject *callback = PyTuple_GET_ITEM((PyObject *)data, 0);
                PyObject *userdata = PyTuple_GET_ITEM((PyObject *)data, 1);
                PyObject *retval = PyObject_CallFunctionObjArgs(callback, buffer, userdata, NULL);
                if(NULL == retval) {
                    PyErr_Print();
                } else {
                    Py_DECREF(retval);
                }
            }
        }
        Py_XDECREF(buffer);
    } else {
        PyErr_Print();
    }

    PySDL_ThreadLeave(gil);
}

static int PySDL_Audio_Type_init(PySDL_Audio *self, PyObject *args, PyObject *kwds) {
    self->deviceId = 0;
    self->stream = NULL;
    self->pycallback = NULL;
    return 0;
}

static void _audio_close(PySDL_Audio *self) {
    if(NULL != self->stream) {
        // SDL_DestroyAudioStream joins the callback thread, which needs the GIL.
        SDL_AudioStream *stream = self->stream;
        Py_BEGIN_ALLOW_THREADS
            SDL_DestroyAudioStream(stream);
        Py_END_ALLOW_THREADS
        self->stream = NULL;
        self->deviceId = 0;
    }
    Py_CLEAR(self->pycallback);
}

static void PySDL_Audio_Type_dealloc(PySDL_Audio *self) {
    _audio_close(self);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * PySDL_Audio_Open(PySDL_Audio *self, PyObject *args, PyObject *kwds) {
    char *deviceName = NULL;  // accepted for compatibility; SDL3 opens by ID
    int capture = 0;
    int freq = 0, format = 0, channels = 0, samples = 0, flags = 0;
    PyObject *callback = NULL;
    PyObject *userdata = NULL;

    if(NULL != self->stream) {
        PyErr_SetString(pysdl_Error, "Audio device already open");
        return NULL;
    }

    static char *kwlist[] = {"deviceName", "capture", "freq", "format", "channels",
                             "samples", "flags", "callback", "userdata", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|ziiiiiiOO", kwlist,
        &deviceName, &capture, &freq, &format, &channels, &samples, &flags,
        &callback, &userdata)) {
        return NULL;
    }
    (void)deviceName; (void)samples; (void)flags;

    SDL_AudioSpec spec = { (SDL_AudioFormat)format, channels, freq };
    SDL_AudioSpec *want = (format || channels || freq) ? &spec : NULL;

    SDL_AudioStreamCallback cb = NULL;
    PyObject *cbtuple = NULL;
    if(NULL != callback) {
        if(!PyCallable_Check(callback)) {
            PyErr_SetString(PyExc_TypeError, "The callback is not a callable object");
            return NULL;
        }
        cb = capture ? _capture_callback : _playback_callback;
        cbtuple = Py_BuildValue("(OO)", callback, userdata ? userdata : Py_None);
        if(NULL == cbtuple) {
            return NULL;
        }
    }

    SDL_AudioDeviceID devid = capture ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
                                      : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    self->stream = SDL_OpenAudioDeviceStream(devid, want, cb, cbtuple);
    if(NULL == self->stream) {
        Py_XDECREF(cbtuple);
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    self->pycallback = cbtuple;  // released in Close / dealloc
    self->deviceId = SDL_GetAudioStreamDevice(self->stream);

    // Report the actual device format (samples has no SDL3 equivalent -> frames).
    SDL_AudioSpec have;
    int frames = 0;
    if(SDL_GetAudioDeviceFormat(self->deviceId, &have, &frames)) {
        return Py_BuildValue("(iiii)", have.freq, have.format, have.channels, frames);
    }
    return Py_BuildValue("(iiii)", freq, format, channels, 0);
}

static PyObject * PySDL_Audio_Close(PySDL_Audio *self, PyObject *ign) {
    _audio_close(self);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_Lock(PySDL_Audio *self, PyObject *ign) {
    if(self->stream) {
        Py_BEGIN_ALLOW_THREADS
            SDL_LockAudioStream(self->stream);
        Py_END_ALLOW_THREADS
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_Unlock(PySDL_Audio *self, PyObject *ign) {
    if(self->stream) {
        SDL_UnlockAudioStream(self->stream);
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_Pause(PySDL_Audio *self, PyObject *arg) {
    int pause = PyObject_IsTrue(arg);
    if(-1 == pause) {
        return NULL;
    }
    bool ok = true;
    Py_BEGIN_ALLOW_THREADS
        ok = pause ? SDL_PauseAudioDevice(self->deviceId)
                   : SDL_ResumeAudioDevice(self->deviceId);
    Py_END_ALLOW_THREADS
    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_Queue(PySDL_Audio *self, PyObject *arg) {
    if(NULL == self->stream) {
        PyErr_SetString(pysdl_Error, "Audio device is not open");
        return NULL;
    }
    Py_buffer buffer;
    if(0 > PyObject_GetBuffer(arg, &buffer, PyBUF_SIMPLE)) {
        return NULL;
    }
    bool ok = SDL_PutAudioStreamData(self->stream, buffer.buf, (int)buffer.len);
    PyBuffer_Release(&buffer);
    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_Dequeue(PySDL_Audio *self, PyObject *arg) {
    if(NULL == self->stream) {
        PyErr_SetString(pysdl_Error, "Audio device is not open");
        return NULL;
    }
    long length = PyLong_AsLong(arg);
    if(-1 == length && PyErr_Occurred()) {
        return NULL;
    }
    if(length < 0) {
        PyErr_SetString(PyExc_ValueError, "length must be >= 0");
        return NULL;
    }

    PyObject *buffer = PyBytes_FromStringAndSize(NULL, length);
    if(NULL == buffer) {
        return NULL;
    }
    int got = SDL_GetAudioStreamData(self->stream, PyBytes_AS_STRING(buffer), (int)length);
    if(0 > got) {
        Py_DECREF(buffer);
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    if(got != length && 0 > _PyBytes_Resize(&buffer, got)) {
        return NULL;
    }
    return buffer;
}

static PyObject * PySDL_Audio_GetQueueSize(PySDL_Audio *self, PyObject *ign) {
    if(NULL == self->stream) {
        return PyLong_FromLong(0);
    }
    return PyLong_FromLong(SDL_GetAudioStreamQueued(self->stream));
}

static PyObject * PySDL_Audio_ClearQueued(PySDL_Audio *self, PyObject *ign) {
    if(self->stream) {
        SDL_ClearAudioStream(self->stream);
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_GetStatus(PySDL_Audio *self, PyObject *ign) {
    // 0 = not open, 1 = playing, 2 = paused
    if(NULL == self->stream) {
        return PyLong_FromLong(0);
    }
    return PyLong_FromLong(SDL_AudioDevicePaused(self->deviceId) ? 2 : 1);
}

//=========================================================
// module functions
//=========================================================

static PyObject * PySDL_GetNumAudioDrivers(PyObject *self, PyObject *ign) {
    return PyLong_FromLong(SDL_GetNumAudioDrivers());
}

static PyObject * PySDL_GetAudioDriver(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_GetAudioDriver((int)index);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_GetCurrentAudioDriver(PyObject *self, PyObject *ign) {
    const char *name = SDL_GetCurrentAudioDriver();
    if(NULL == name) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(name);
}

static PyObject * PySDL_LoadWAV(PyObject *self, PyObject *arg) {
    SDL_AudioSpec spec;
    SDL_memset(&spec, 0, sizeof(spec));
    Uint8 *audio = NULL;
    Uint32 length = 0;
    bool ok;

    if(PyBytes_Check(arg) || PyByteArray_Check(arg)) {
        Py_buffer bytes;
        if(0 > PyObject_GetBuffer(arg, &bytes, PyBUF_SIMPLE)) {
            return NULL;
        }
        SDL_IOStream *io = SDL_IOFromConstMem(bytes.buf, (size_t)bytes.len);
        if(NULL == io) {
            PyBuffer_Release(&bytes);
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return NULL;
        }
        ok = SDL_LoadWAV_IO(io, true, &spec, &audio, &length);
        PyBuffer_Release(&bytes);
    } else {
        const char *path = PyUnicode_AsUTF8(arg);
        if(NULL == path) {
            return NULL;
        }
        ok = SDL_LoadWAV(path, &spec, &audio, &length);
    }

    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    PyObject *data = PyBytes_FromStringAndSize((char *)audio, length);
    SDL_free(audio);
    if(NULL == data) {
        return NULL;
    }
    return Py_BuildValue("((iii)N)", spec.freq, spec.format, spec.channels, data);
}

static PyObject * PySDL_MixAudioFormat(PyObject *self, PyObject *args) {
    Py_buffer dst, src;
    int format;
    double volume = 1.0;

    if(!PyArg_ParseTuple(args, "y*y*i|d", &dst, &src, &format, &volume)) {
        return NULL;
    }

    Py_ssize_t length = dst.len < src.len ? dst.len : src.len;
    PyObject *out = PyBytes_FromStringAndSize((const char *)dst.buf, dst.len);
    PyBuffer_Release(&dst);
    if(NULL == out) {
        PyBuffer_Release(&src);
        return NULL;
    }

    SDL_MixAudio((Uint8 *)PyBytes_AS_STRING(out), src.buf,
        (SDL_AudioFormat)format, (Uint32)length, (float)volume);
    PyBuffer_Release(&src);
    return out;
}

static SDL_AudioDeviceID * _audio_device_list(int iscapture, int *count) {
    return iscapture ? SDL_GetAudioRecordingDevices(count)
                     : SDL_GetAudioPlaybackDevices(count);
}

static PyObject * PySDL_GetAudioDeviceSpec(PyObject *self, PyObject *args) {
    int index;
    int iscapture = 0;
    if(!PyArg_ParseTuple(args, "i|p", &index, &iscapture)) {
        return NULL;
    }
    int count = 0;
    SDL_AudioDeviceID *ids = _audio_device_list(iscapture, &count);
    if(NULL == ids || index < 0 || index >= count) {
        SDL_free(ids);
        PyErr_SetString(PyExc_IndexError, "audio device index out of range");
        return NULL;
    }
    SDL_AudioSpec spec;
    int frames = 0;
    bool ok = SDL_GetAudioDeviceFormat(ids[index], &spec, &frames);
    SDL_free(ids);
    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return Py_BuildValue("(iiii)", spec.freq, spec.format, spec.channels, frames);
}

static PyObject * PySDL_GetDefaultAudioInfo(PyObject *self, PyObject *args) {
    int iscapture = 0;
    if(!PyArg_ParseTuple(args, "|p", &iscapture)) {
        return NULL;
    }
    SDL_AudioDeviceID devid = iscapture ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
                                        : SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    SDL_AudioSpec spec;
    int frames = 0;
    if(!SDL_GetAudioDeviceFormat(devid, &spec, &frames)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    const char *name = SDL_GetAudioDeviceName(devid);
    return Py_BuildValue("(s(iiii))", name ? name : "",
        spec.freq, spec.format, spec.channels, frames);
}

PyMethodDef pysdl_audio_methods[] = {
    { "GetNumAudioDrivers",    PySDL_GetNumAudioDrivers,    METH_NOARGS  },
    { "GetAudioDriver",        PySDL_GetAudioDriver,        METH_O       },
    { "GetCurrentAudioDriver", PySDL_GetCurrentAudioDriver, METH_NOARGS  },
    { "LoadWAV",               PySDL_LoadWAV,               METH_O       },
    { "MixAudioFormat",        PySDL_MixAudioFormat,        METH_VARARGS },
    { "MixAudio",              PySDL_MixAudioFormat,        METH_VARARGS },
    { "GetAudioDeviceSpec",    PySDL_GetAudioDeviceSpec,    METH_VARARGS },
    { "GetDefaultAudioInfo",   PySDL_GetDefaultAudioInfo,   METH_VARARGS },
    { NULL }
};
