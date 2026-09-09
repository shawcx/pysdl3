#include "pysdl.h"

static int        PySDL_AudioStream_Type_init    (PySDL_AudioStream*, PyObject*, PyObject*);
static void       PySDL_AudioStream_Type_dealloc (PySDL_AudioStream*);

static PyObject * PySDL_AudioStream_Put       (PySDL_AudioStream*, PyObject*);
static PyObject * PySDL_AudioStream_Get       (PySDL_AudioStream*, PyObject*);
static PyObject * PySDL_AudioStream_Available (PySDL_AudioStream*, PyObject*);
static PyObject * PySDL_AudioStream_Flush     (PySDL_AudioStream*, PyObject*);
static PyObject * PySDL_AudioStream_Clear     (PySDL_AudioStream*, PyObject*);

static PyMethodDef PySDL_AudioStream_methods[] = {
    { "Put",       (PyCFunction)PySDL_AudioStream_Put,       METH_O      },
    { "Get",       (PyCFunction)PySDL_AudioStream_Get,       METH_O      },
    { "Available", (PyCFunction)PySDL_AudioStream_Available, METH_NOARGS },
    { "Flush",     (PyCFunction)PySDL_AudioStream_Flush,     METH_NOARGS },
    { "Clear",     (PyCFunction)PySDL_AudioStream_Clear,     METH_NOARGS },
    { NULL }
};

PyTypeObject PySDL_AudioStream_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.AudioStream",
    .tp_basicsize = sizeof(PySDL_AudioStream),
    .tp_dealloc   = (destructor)PySDL_AudioStream_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.AudioStream(src_format, src_channels, src_rate, dst_format, dst_channels, dst_rate)",
    .tp_methods   = PySDL_AudioStream_methods,
    .tp_init      = (initproc)PySDL_AudioStream_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_AudioStream_Type_init(PySDL_AudioStream *self, PyObject *args, PyObject *kwds) {
    int src_format = 0, src_channels = 0, src_rate = 0;
    int dst_format = 0, dst_channels = 0, dst_rate = 0;

    static char *kwlist[] = {"src_format", "src_channels", "src_rate",
                             "dst_format", "dst_channels", "dst_rate", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|iiiiii", kwlist,
        &src_format, &src_channels, &src_rate, &dst_format, &dst_channels, &dst_rate)) {
        return -1;
    }

    self->stream = NULL;
    if(src_rate > 0 && dst_rate > 0) {
        SDL_AudioSpec src = { (SDL_AudioFormat)src_format, src_channels, src_rate };
        SDL_AudioSpec dst = { (SDL_AudioFormat)dst_format, dst_channels, dst_rate };
        self->stream = SDL_CreateAudioStream(&src, &dst);
        if(NULL == self->stream) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_AudioStream_Type_dealloc(PySDL_AudioStream *self) {
    if(NULL != self->stream) {
        SDL_DestroyAudioStream(self->stream);
        self->stream = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static SDL_AudioStream * _stream(PySDL_AudioStream *self) {
    if(NULL == self->stream) {
        PyErr_SetString(pysdl_Error, "AudioStream is not initialized");
        return NULL;
    }
    return self->stream;
}

static PyObject * PySDL_AudioStream_Put(PySDL_AudioStream *self, PyObject *arg) {
    SDL_AudioStream *stream = _stream(self);
    if(NULL == stream) {
        return NULL;
    }

    Py_buffer buffer;
    if(0 > PyObject_GetBuffer(arg, &buffer, PyBUF_SIMPLE)) {
        return NULL;
    }
    bool ok = SDL_PutAudioStreamData(stream, buffer.buf, (int)buffer.len);
    PyBuffer_Release(&buffer);
    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_AudioStream_Get(PySDL_AudioStream *self, PyObject *arg) {
    SDL_AudioStream *stream = _stream(self);
    long want = PyLong_AsLong(arg);
    if(NULL == stream || (-1 == want && PyErr_Occurred())) {
        return NULL;
    }
    if(want < 0) {
        PyErr_SetString(PyExc_ValueError, "length must be >= 0");
        return NULL;
    }

    PyObject *out = PyBytes_FromStringAndSize(NULL, want);
    if(NULL == out) {
        return NULL;
    }
    int got = SDL_GetAudioStreamData(stream, PyBytes_AS_STRING(out), (int)want);
    if(0 > got) {
        Py_DECREF(out);
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    if(got != want && 0 > _PyBytes_Resize(&out, got)) {
        return NULL;
    }
    return out;
}

static PyObject * PySDL_AudioStream_Available(PySDL_AudioStream *self, PyObject *ign) {
    SDL_AudioStream *stream = _stream(self);
    return stream ? PyLong_FromLong(SDL_GetAudioStreamAvailable(stream)) : NULL;
}

static PyObject * PySDL_AudioStream_Flush(PySDL_AudioStream *self, PyObject *ign) {
    SDL_AudioStream *stream = _stream(self);
    if(NULL == stream) {
        return NULL;
    }
    if(!SDL_FlushAudioStream(stream)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_AudioStream_Clear(PySDL_AudioStream *self, PyObject *ign) {
    SDL_AudioStream *stream = _stream(self);
    if(NULL == stream) {
        return NULL;
    }
    SDL_ClearAudioStream(stream);
    Py_RETURN_NONE;
}
