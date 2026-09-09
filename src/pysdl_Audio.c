#include "pysdl.h"

static int        PySDL_Audio_Type_init    (PySDL_Audio*, PyObject*, PyObject*);
static void       PySDL_Audio_Type_dealloc (PySDL_Audio* );

PyObject * PySDL_Audio_getter (PyObject*, void*);
int        PySDL_Audio_setter (PyObject*, PyObject*, void*);

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
    .tp_doc       = "SDL3.Audio Class",
    .tp_methods   = PySDL_Audio_methods,
    .tp_init      = (initproc)PySDL_Audio_Type_init,
    .tp_new       = PyType_GenericNew
};

static void _playback_callback(void *data, Uint8 *stream, int len) {
    PyGILState_STATE gil;

    // Silence is the fallback whenever the Python callback does not deliver a
    // full buffer (error, wrong type, short bytes).
    SDL_memset(stream, 0, len);

    if(!PySDL_ThreadEnter(&gil)) {
        return;
    }

    PyObject *callback = PyTuple_GET_ITEM((PyObject *)data, 0);
    PyObject *userdata = PyTuple_GET_ITEM((PyObject *)data, 1);
    PyObject *size     = PyLong_FromLong(len);

    PyObject *audioData = PyObject_CallFunctionObjArgs(callback, size, userdata, NULL);
    if(NULL == audioData) {
        PyErr_Print();
    } else if(!PyBytes_Check(audioData)) {
        PyErr_Format(PyExc_TypeError, "audio callback must return bytes, not %s", Py_TYPE(audioData)->tp_name);
        PyErr_Print();
        Py_DECREF(audioData);
    } else {
        Py_ssize_t have = PyBytes_GET_SIZE(audioData);
        memcpy(stream, PyBytes_AS_STRING(audioData), have < len ? (size_t)have : (size_t)len);
        Py_DECREF(audioData);
    }

    Py_XDECREF(size);

    PySDL_ThreadLeave(gil);
}

static void _capture_callback(void *data, Uint8 *stream, int len) {
    PyGILState_STATE gil;

    if(!PySDL_ThreadEnter(&gil)) {
        return;
    }

    PyObject *callback = PyTuple_GET_ITEM((PyObject *)data, 0);
    PyObject *userdata = PyTuple_GET_ITEM((PyObject *)data, 1);

    PyObject *audioData = PyBytes_FromStringAndSize((char *)stream, len);
    if(NULL != audioData) {
        PyObject *retval = PyObject_CallFunctionObjArgs(callback, audioData, userdata, NULL);
        if(NULL == retval) {
            PyErr_Print();
        } else {
            Py_DECREF(retval);
        }
        Py_DECREF(audioData);
    } else {
        PyErr_Print();
    }

    PySDL_ThreadLeave(gil);
}

static int PySDL_Audio_Type_init(PySDL_Audio *self, PyObject *args, PyObject *kwds) {
    self->deviceId = 0;
    self->pycallback = NULL;
    return 0;
}

static void PySDL_Audio_Type_dealloc(PySDL_Audio *self) {
    if(0 != self->deviceId) {
        Py_BEGIN_ALLOW_THREADS
            SDL_CloseAudioDevice(self->deviceId);
        Py_END_ALLOW_THREADS
        self->deviceId = 0;
    }
    Py_XDECREF(self->pycallback);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * PySDL_Audio_Open(PySDL_Audio *self, PyObject *args, PyObject *kwds) {
    char *deviceName = NULL;
    int capture = 0;
    SDL_AudioSpec want;
    SDL_AudioSpec have;
    int flags = SDL_AUDIO_ALLOW_ANY_CHANGE;
    PyObject *callback = NULL;
    PyObject *userdata = NULL;

    if(0 != self->deviceId) {
        PyErr_SetString(pysdl_Error, "Audio device already open");
        return NULL;
    }

    SDL_memset(&want, 0, sizeof(want));
    SDL_memset(&have, 0, sizeof(have));

    //want.freq     = 48000;
    //want.format   = AUDIO_S16LSB;
    //want.channels = 2;
    //want.samples  = 4096;

    // deviceName defaults to NULL: the system's default output/capture device.
    static char *kwlist[] = {"deviceName", "capture", "freq", "format", "channels", "samples", "flags", "callback", "userdata", NULL};
    int ok = PyArg_ParseTupleAndKeywords(args, kwds, "|ziiiiiiOO", kwlist,
        &deviceName, &capture, &want.freq, &want.format, &want.channels, &want.samples, &flags, &callback, &userdata);
    if(!ok) {
        return NULL;
    }
    if(deviceName && deviceName[0] == '\0') {
        deviceName = NULL;  // "" is not a valid device name; treat it as default
    }

    PyObject *cbtuple = NULL;

    if(NULL != callback) {
        if(!PyCallable_Check(callback)) {
            PyErr_SetString(PyExc_TypeError, "The callback is not a callable object");
            return NULL;
        }

        // set the C callback; the tuple owns the only references SDL's audio
        // thread holds and is released in Close/dealloc.
        want.callback = capture ? _capture_callback : _playback_callback;
        cbtuple = Py_BuildValue("(OO)", callback, userdata ? userdata : Py_None);
        if(NULL == cbtuple) {
            return NULL;
        }
        want.userdata = cbtuple;
    }

    self->deviceId = SDL_OpenAudioDevice(deviceName, capture, &want, &have, flags);
    if(0 == self->deviceId) {
        Py_XDECREF(cbtuple);
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    self->pycallback = cbtuple;

    return Py_BuildValue("(iiii)", have.freq, have.format, have.channels, have.samples);
}

static PyObject * PySDL_Audio_Close(PySDL_Audio *self, PyObject *ign) {
    if(0 != self->deviceId) {
        // Drop the GIL: SDL_CloseAudioDevice joins the callback thread, which
        // is itself trying to re-acquire the GIL.
        Py_BEGIN_ALLOW_THREADS
            SDL_CloseAudioDevice(self->deviceId);
        Py_END_ALLOW_THREADS
        self->deviceId = 0;
    }
    Py_CLEAR(self->pycallback);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_Lock(PySDL_Audio *self, PyObject *ign) {
    Py_BEGIN_ALLOW_THREADS
        SDL_LockAudioDevice(self->deviceId);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_Unlock(PySDL_Audio *self, PyObject *ign) {
    SDL_UnlockAudioDevice(self->deviceId);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_Pause(PySDL_Audio *self, PyObject *arg) {
    int pause = PyObject_IsTrue(arg);
    if(-1 == pause) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
        SDL_PauseAudioDevice(self->deviceId, pause);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_Queue(PySDL_Audio *self, PyObject *arg) {
    Py_buffer buffer;
    if(0 > PyObject_GetBuffer(arg, &buffer, PyBUF_SIMPLE)) {
        return NULL;
    }
    int rc = SDL_QueueAudio(self->deviceId, buffer.buf, (Uint32)buffer.len);
    PyBuffer_Release(&buffer);
    if(0 > rc) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_Dequeue(PySDL_Audio *self, PyObject *arg) {
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
    Uint32 returned = SDL_DequeueAudio(self->deviceId, PyBytes_AS_STRING(buffer), (Uint32)length);
    if((Uint32)length != returned && 0 > _PyBytes_Resize(&buffer, returned)) {
        return NULL;
    }
    return buffer;
}

static PyObject * PySDL_Audio_GetQueueSize(PySDL_Audio *self, PyObject *ign) {
    return PyLong_FromUnsignedLong(SDL_GetQueuedAudioSize(self->deviceId));
}

static PyObject * PySDL_Audio_ClearQueued(PySDL_Audio *self, PyObject *ign) {
    SDL_ClearQueuedAudio(self->deviceId);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Audio_GetStatus(PySDL_Audio *self, PyObject *ign) {
    return PyLong_FromLong(SDL_GetAudioDeviceStatus(self->deviceId));
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

static PyObject * PySDL_AudioInit(PyObject *self, PyObject *arg) {
    const char *driver = PyUnicode_AsUTF8(arg);
    if(NULL == driver) {
        return NULL;
    }
    if(0 > SDL_AudioInit(driver)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_AudioQuit(PyObject *self, PyObject *ign) {
    SDL_AudioQuit();
    Py_RETURN_NONE;
}

static PyObject * PySDL_LoadWAV(PyObject *self, PyObject *arg) {
    SDL_RWops *rw = NULL;
    Py_buffer bytes;
    bytes.obj = NULL;

    if(PyBytes_Check(arg) || PyByteArray_Check(arg)) {
        if(0 > PyObject_GetBuffer(arg, &bytes, PyBUF_SIMPLE)) {
            return NULL;
        }
        rw = SDL_RWFromConstMem(bytes.buf, (int)bytes.len);
    } else {
        const char *path = PyUnicode_AsUTF8(arg);
        if(NULL == path) {
            return NULL;
        }
        rw = SDL_RWFromFile(path, "rb");
    }
    if(NULL == rw) {
        if(bytes.obj) {
            PyBuffer_Release(&bytes);
        }
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    SDL_AudioSpec spec;
    Uint8 *audio = NULL;
    Uint32 length = 0;
    SDL_AudioSpec *result = SDL_LoadWAV_RW(rw, 1, &spec, &audio, &length);
    if(bytes.obj) {
        PyBuffer_Release(&bytes);
    }
    if(NULL == result) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    PyObject *data = PyBytes_FromStringAndSize((char *)audio, length);
    SDL_FreeWAV(audio);
    if(NULL == data) {
        return NULL;
    }
    return Py_BuildValue("((iiii)N)", spec.freq, spec.format, spec.channels, spec.samples, data);
}

static PyObject * PySDL_MixAudioFormat(PyObject *self, PyObject *args) {
    Py_buffer dst, src;
    int format;
    int volume = SDL_MIX_MAXVOLUME;

    if(!PyArg_ParseTuple(args, "y*y*i|i", &dst, &src, &format, &volume)) {
        return NULL;
    }

    Py_ssize_t length = dst.len < src.len ? dst.len : src.len;
    PyObject *out = PyBytes_FromStringAndSize((const char *)dst.buf, dst.len);
    PyBuffer_Release(&dst);
    if(NULL == out) {
        PyBuffer_Release(&src);
        return NULL;
    }

    SDL_MixAudioFormat((Uint8 *)PyBytes_AS_STRING(out), src.buf,
        (SDL_AudioFormat)format, (Uint32)length, volume);
    PyBuffer_Release(&src);
    return out;
}

#if SDL_VERSION_ATLEAST(2,0,16)
static PyObject * PySDL_GetAudioDeviceSpec(PyObject *self, PyObject *args) {
    int index;
    int iscapture = 0;
    if(!PyArg_ParseTuple(args, "i|p", &index, &iscapture)) {
        return NULL;
    }
    SDL_AudioSpec spec;
    SDL_memset(&spec, 0, sizeof(spec));
    if(0 > SDL_GetAudioDeviceSpec(index, iscapture, &spec)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return Py_BuildValue("(iiii)", spec.freq, spec.format, spec.channels, spec.samples);
}
#endif

#if SDL_VERSION_ATLEAST(2,24,0)
static PyObject * PySDL_GetDefaultAudioInfo(PyObject *self, PyObject *args) {
    int iscapture = 0;
    if(!PyArg_ParseTuple(args, "|p", &iscapture)) {
        return NULL;
    }
    char *name = NULL;
    SDL_AudioSpec spec;
    SDL_memset(&spec, 0, sizeof(spec));
    if(0 > SDL_GetDefaultAudioInfo(&name, &spec, iscapture)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    PyObject *result = Py_BuildValue("(s(iiii))", name ? name : "",
        spec.freq, spec.format, spec.channels, spec.samples);
    SDL_free(name);
    return result;
}
#endif

PyMethodDef pysdl_audio_methods[] = {
    { "GetNumAudioDrivers",   PySDL_GetNumAudioDrivers,   METH_NOARGS  },
    { "GetAudioDriver",       PySDL_GetAudioDriver,       METH_O       },
    { "GetCurrentAudioDriver", PySDL_GetCurrentAudioDriver, METH_NOARGS },
    { "AudioInit",            PySDL_AudioInit,            METH_O       },
    { "AudioQuit",            PySDL_AudioQuit,            METH_NOARGS  },
    { "LoadWAV",              PySDL_LoadWAV,              METH_O       },
    { "MixAudioFormat",       PySDL_MixAudioFormat,       METH_VARARGS },
#if SDL_VERSION_ATLEAST(2,0,16)
    { "GetAudioDeviceSpec",   PySDL_GetAudioDeviceSpec,   METH_VARARGS },
#endif
#if SDL_VERSION_ATLEAST(2,24,0)
    { "GetDefaultAudioInfo",  PySDL_GetDefaultAudioInfo,  METH_VARARGS },
#endif
    { NULL }
};

