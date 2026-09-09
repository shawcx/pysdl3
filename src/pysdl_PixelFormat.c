#include "pysdl.h"

static int        PySDL_PixelFormat_Type_init    (PySDL_PixelFormat*, PyObject*, PyObject*);
static void       PySDL_PixelFormat_Type_dealloc (PySDL_PixelFormat*);

static PyObject * PySDL_PixelFormat_MapRGB     (PySDL_PixelFormat*, PyObject*);
static PyObject * PySDL_PixelFormat_MapRGBA    (PySDL_PixelFormat*, PyObject*);
static PyObject * PySDL_PixelFormat_GetRGB     (PySDL_PixelFormat*, PyObject*);
static PyObject * PySDL_PixelFormat_GetRGBA    (PySDL_PixelFormat*, PyObject*);
static PyObject * PySDL_PixelFormat_SetPalette (PySDL_PixelFormat*, PyObject*);

static PyMethodDef PySDL_PixelFormat_methods[] = {
    { "MapRGB",     (PyCFunction)PySDL_PixelFormat_MapRGB,     METH_O },
    { "MapRGBA",    (PyCFunction)PySDL_PixelFormat_MapRGBA,    METH_O },
    { "GetRGB",     (PyCFunction)PySDL_PixelFormat_GetRGB,     METH_O },
    { "GetRGBA",    (PyCFunction)PySDL_PixelFormat_GetRGBA,    METH_O },
    { "SetPalette", (PyCFunction)PySDL_PixelFormat_SetPalette, METH_O },
    { NULL }
};

#define PF_FORMAT 0
#define PF_BPP    1
#define PF_BYTES  2
#define PF_RMASK  3
#define PF_GMASK  4
#define PF_BMASK  5
#define PF_AMASK  6

// Every accessor goes through this: SDL_Alloc/Map/GetRGB do not tolerate a NULL
// format, and `SDL2.PixelFormat()` (no arg) leaves one.
static SDL_PixelFormat * _fmt(PySDL_PixelFormat *self) {
    if(NULL == self->format) {
        PyErr_SetString(pysdl_Error, "PixelFormat is not initialized");
        return NULL;
    }
    return self->format;
}

static PyObject * PySDL_PixelFormat_getter(PyObject *self, void *param) {
    SDL_PixelFormat *fmt = _fmt((PySDL_PixelFormat *)self);
    if(NULL == fmt) {
        return NULL;
    }
    switch((long)param) {
    case PF_FORMAT: return PyLong_FromUnsignedLong(fmt->format);
    case PF_BPP:    return PyLong_FromLong(fmt->BitsPerPixel);
    case PF_BYTES:  return PyLong_FromLong(fmt->BytesPerPixel);
    case PF_RMASK:  return PyLong_FromUnsignedLong(fmt->Rmask);
    case PF_GMASK:  return PyLong_FromUnsignedLong(fmt->Gmask);
    case PF_BMASK:  return PyLong_FromUnsignedLong(fmt->Bmask);
    case PF_AMASK:  return PyLong_FromUnsignedLong(fmt->Amask);
    }
    Py_RETURN_NONE;
}

static PyGetSetDef PySDL_PixelFormat_getset[] = {
    { "format", PySDL_PixelFormat_getter, NULL, "pixel-format enum", (void*)PF_FORMAT },
    { "bpp",    PySDL_PixelFormat_getter, NULL, "bits per pixel",    (void*)PF_BPP    },
    { "bytes",  PySDL_PixelFormat_getter, NULL, "bytes per pixel",   (void*)PF_BYTES  },
    { "Rmask",  PySDL_PixelFormat_getter, NULL, "", (void*)PF_RMASK },
    { "Gmask",  PySDL_PixelFormat_getter, NULL, "", (void*)PF_GMASK },
    { "Bmask",  PySDL_PixelFormat_getter, NULL, "", (void*)PF_BMASK },
    { "Amask",  PySDL_PixelFormat_getter, NULL, "", (void*)PF_AMASK },
    { NULL }
};

PyTypeObject PySDL_PixelFormat_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.PixelFormat",
    .tp_basicsize = sizeof(PySDL_PixelFormat),
    .tp_dealloc   = (destructor)PySDL_PixelFormat_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.PixelFormat(format_enum)",
    .tp_methods   = PySDL_PixelFormat_methods,
    .tp_getset    = PySDL_PixelFormat_getset,
    .tp_init      = (initproc)PySDL_PixelFormat_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_PixelFormat_Type_init(PySDL_PixelFormat *self, PyObject *args, PyObject *kwds) {
    unsigned int format = 0;
    if(!PyArg_ParseTuple(args, "|I", &format)) {
        return -1;
    }

    self->format = NULL;
    if(format) {
        self->format = SDL_AllocFormat(format);
        if(NULL == self->format) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_PixelFormat_Type_dealloc(PySDL_PixelFormat *self) {
    if(NULL != self->format) {
        SDL_FreeFormat(self->format);
        self->format = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * PySDL_PixelFormat_MapRGB(PySDL_PixelFormat *self, PyObject *arg) {
    SDL_PixelFormat *fmt = _fmt(self);
    SDL_Color c;
    if(NULL == fmt || !PyToColor(arg, &c)) {
        return NULL;
    }
    return PyLong_FromUnsignedLong(SDL_MapRGB(fmt, c.r, c.g, c.b));
}

static PyObject * PySDL_PixelFormat_MapRGBA(PySDL_PixelFormat *self, PyObject *arg) {
    SDL_PixelFormat *fmt = _fmt(self);
    SDL_Color c;
    if(NULL == fmt || !PyToColor(arg, &c)) {
        return NULL;
    }
    return PyLong_FromUnsignedLong(SDL_MapRGBA(fmt, c.r, c.g, c.b, c.a));
}

static PyObject * PySDL_PixelFormat_GetRGB(PySDL_PixelFormat *self, PyObject *arg) {
    SDL_PixelFormat *fmt = _fmt(self);
    if(NULL == fmt) {
        return NULL;
    }
    unsigned long pixel = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == pixel && PyErr_Occurred()) {
        return NULL;
    }
    Uint8 r, g, b;
    SDL_GetRGB((Uint32)pixel, fmt, &r, &g, &b);
    return Py_BuildValue("(iii)", r, g, b);
}

static PyObject * PySDL_PixelFormat_GetRGBA(PySDL_PixelFormat *self, PyObject *arg) {
    SDL_PixelFormat *fmt = _fmt(self);
    if(NULL == fmt) {
        return NULL;
    }
    unsigned long pixel = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == pixel && PyErr_Occurred()) {
        return NULL;
    }
    Uint8 r, g, b, a;
    SDL_GetRGBA((Uint32)pixel, fmt, &r, &g, &b, &a);
    return Py_BuildValue("(iiii)", r, g, b, a);
}

static PyObject * PySDL_PixelFormat_SetPalette(PySDL_PixelFormat *self, PyObject *arg) {
    SDL_PixelFormat *fmt = _fmt(self);
    if(NULL == fmt) {
        return NULL;
    }
    if(!PyObject_TypeCheck(arg, &PySDL_Palette_Type)) {
        PyErr_SetString(PyExc_TypeError, "expected an SDL2.Palette");
        return NULL;
    }
    if(0 > SDL_SetPixelFormatPalette(fmt, ((PySDL_Palette *)arg)->palette)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}
