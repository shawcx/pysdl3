#include "pysdl.h"

static int        PySDL_Surface_Type_init    (PySDL_Surface*, PyObject*, PyObject*);
static void       PySDL_Surface_Type_dealloc (PySDL_Surface*);

static PyObject * PySDL_Surface_LockSurface   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_UnlockSurface (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SaveBMP       (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SavePNG       (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SaveJPG       (PySDL_Surface*, PyObject*, PyObject*);

static PyObject * PySDL_Surface_Blit          (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_BlitScaled    (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_SoftStretch   (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_FillRect      (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_FillRects     (PySDL_Surface*, PyObject*);

static PyObject * PySDL_Surface_MapRGB        (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_MapRGBA       (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_GetPixelFormat(PySDL_Surface*, PyObject*);

static PyObject * PySDL_Surface_SetColorKey   (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_GetColorKey   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetBlendMode  (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_GetBlendMode  (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetColorMod   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_GetColorMod   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetAlphaMod   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_GetAlphaMod   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetClipRect   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_GetClipRect   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetRLE        (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetPalette    (PySDL_Surface*, PyObject*);

static PyObject * PySDL_Surface_Convert       (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_Duplicate     (PySDL_Surface*, PyObject*);

static PyMethodDef PySDL_Surface_methods[] = {
    { "LockSurface",    (PyCFunction)PySDL_Surface_LockSurface,    METH_NOARGS  },
    { "UnlockSurface",  (PyCFunction)PySDL_Surface_UnlockSurface,  METH_NOARGS  },
    { "SaveBMP",        (PyCFunction)PySDL_Surface_SaveBMP,        METH_O       },
    { "SavePNG",        (PyCFunction)PySDL_Surface_SavePNG,        METH_O       },
    { "SaveJPG",        (PyCFunction)PySDL_Surface_SaveJPG,        METH_VARARGS | METH_KEYWORDS },

    { "Blit",           (PyCFunction)PySDL_Surface_Blit,           METH_VARARGS | METH_KEYWORDS },
    { "BlitScaled",     (PyCFunction)PySDL_Surface_BlitScaled,     METH_VARARGS | METH_KEYWORDS },
    { "SoftStretch",    (PyCFunction)PySDL_Surface_SoftStretch,    METH_VARARGS | METH_KEYWORDS },
    { "FillRect",       (PyCFunction)PySDL_Surface_FillRect,       METH_VARARGS },
    { "FillRects",      (PyCFunction)PySDL_Surface_FillRects,      METH_VARARGS },

    { "MapRGB",         (PyCFunction)PySDL_Surface_MapRGB,         METH_O       },
    { "MapRGBA",        (PyCFunction)PySDL_Surface_MapRGBA,        METH_O       },
    { "GetPixelFormat", (PyCFunction)PySDL_Surface_GetPixelFormat, METH_NOARGS  },

    { "SetColorKey",    (PyCFunction)PySDL_Surface_SetColorKey,    METH_VARARGS | METH_KEYWORDS },
    { "GetColorKey",    (PyCFunction)PySDL_Surface_GetColorKey,    METH_NOARGS  },
    { "SetBlendMode",   (PyCFunction)PySDL_Surface_SetBlendMode,   METH_O       },
    { "GetBlendMode",   (PyCFunction)PySDL_Surface_GetBlendMode,   METH_NOARGS  },
    { "SetColorMod",    (PyCFunction)PySDL_Surface_SetColorMod,    METH_VARARGS },
    { "GetColorMod",    (PyCFunction)PySDL_Surface_GetColorMod,    METH_NOARGS  },
    { "SetAlphaMod",    (PyCFunction)PySDL_Surface_SetAlphaMod,    METH_O       },
    { "GetAlphaMod",    (PyCFunction)PySDL_Surface_GetAlphaMod,    METH_NOARGS  },
    { "SetClipRect",    (PyCFunction)PySDL_Surface_SetClipRect,    METH_O       },
    { "GetClipRect",    (PyCFunction)PySDL_Surface_GetClipRect,    METH_NOARGS  },
    { "SetRLE",         (PyCFunction)PySDL_Surface_SetRLE,         METH_O       },
    { "SetPalette",     (PyCFunction)PySDL_Surface_SetPalette,     METH_O       },

    { "Convert",        (PyCFunction)PySDL_Surface_Convert,        METH_VARARGS },
    { "Duplicate",      (PyCFunction)PySDL_Surface_Duplicate,      METH_NOARGS  },
    { NULL }
};

#define PySDL_SURFACE_WIDTH  0
#define PySDL_SURFACE_HEIGHT 1
#define PySDL_SURFACE_BPP    2
#define PySDL_SURFACE_FORMAT 3
#define PySDL_SURFACE_PIXELS 4
#define PySDL_SURFACE_PITCH  5

static PyObject * PySDL_Surface_getter (PyObject*, void*);
static int        PySDL_Surface_setter (PyObject*, PyObject*, void*);

static PyGetSetDef PySDL_Surface_getset[] = {
    { "w",      PySDL_Surface_getter, NULL,                 "", (void*)PySDL_SURFACE_WIDTH  },
    { "h",      PySDL_Surface_getter, NULL,                 "", (void*)PySDL_SURFACE_HEIGHT },
    { "bpp",    PySDL_Surface_getter, NULL,                 "", (void*)PySDL_SURFACE_BPP    },
    { "pitch",  PySDL_Surface_getter, NULL,                 "", (void*)PySDL_SURFACE_PITCH  },
    { "format", PySDL_Surface_getter, NULL,                 "", (void*)PySDL_SURFACE_FORMAT },
    { "pixels", PySDL_Surface_getter, PySDL_Surface_setter, "", (void*)PySDL_SURFACE_PIXELS },
    { NULL }
};

PyTypeObject PySDL_Surface_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Surface",
    .tp_basicsize = sizeof(PySDL_Surface),
    .tp_dealloc   = (destructor)PySDL_Surface_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Surface Class",
    .tp_methods   = PySDL_Surface_methods,
    .tp_getset    = PySDL_Surface_getset,
    .tp_init      = (initproc)PySDL_Surface_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_Surface_Type_init(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    self->surface = NULL;
    self->shouldFree = 1;
    self->pixels.obj = NULL;
    return 0;
}

static void PySDL_Surface_Type_dealloc(PySDL_Surface *self) {
    if(NULL != self->surface) {
        if(self->shouldFree) {
            SDL_FreeSurface(self->surface);
        }
        self->surface = NULL;
    }
    if(NULL != self->pixels.obj) {
        PyBuffer_Release(&self->pixels);
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * _raise(void) {
    PyErr_SetString(pysdl_Error, SDL_GetError());
    return NULL;
}

// Wrap a freshly created SDL_Surface (ownership transferred).
static PyObject * _wrap(SDL_Surface *surface) {
    if(NULL == surface) {
        return _raise();
    }
    PySDL_Surface *wrapper = (PySDL_Surface *)PySDL_New(&PySDL_Surface_Type);
    if(NULL == wrapper) {
        SDL_FreeSurface(surface);
        return NULL;
    }
    wrapper->surface = surface;
    return (PyObject *)wrapper;
}

//=========================================================
// attributes
//=========================================================

static PyObject * PySDL_Surface_getter(PyObject *self, void *param) {
    SDL_Surface *surface = ((PySDL_Surface *)self)->surface;

    switch((long)param) {
    case PySDL_SURFACE_WIDTH:  return PyLong_FromLong(surface->w);
    case PySDL_SURFACE_HEIGHT: return PyLong_FromLong(surface->h);
    case PySDL_SURFACE_BPP:    return PyLong_FromLong(surface->format->BitsPerPixel);
    case PySDL_SURFACE_PITCH:  return PyLong_FromLong(surface->pitch);
    case PySDL_SURFACE_FORMAT: return PyLong_FromUnsignedLong(surface->format->format);
    case PySDL_SURFACE_PIXELS:
        return PyBytes_FromStringAndSize(surface->pixels, (Py_ssize_t)surface->pitch * surface->h);
    }

    Py_RETURN_NONE;
}

static int PySDL_Surface_setter(PyObject *self, PyObject *value, void *param) {
    SDL_Surface *surface = ((PySDL_Surface *)self)->surface;
    char *buff;
    Py_ssize_t len;

    if((long)param != PySDL_SURFACE_PIXELS) {
        return 0;
    }

    if(0 > PyBytes_AsStringAndSize(value, &buff, &len)) {
        return -1;
    }

    Py_ssize_t size = (Py_ssize_t)surface->pitch * surface->h;
    if(len > size) {
        PyErr_SetString(PyExc_ValueError, "too many bytes for this surface");
        return -1;
    }

    SDL_LockSurface(surface);
    memcpy(surface->pixels, buff, len);
    SDL_UnlockSurface(surface);
    return 0;
}

//=========================================================
// locking / saving
//=========================================================

static PyObject * PySDL_Surface_LockSurface(PySDL_Surface *self, PyObject *ign) {
    if(0 > SDL_LockSurface(self->surface)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_UnlockSurface(PySDL_Surface *self, PyObject *ign) {
    SDL_UnlockSurface(self->surface);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_SaveBMP(PySDL_Surface *self, PyObject *arg) {
    const char *path = PyUnicode_AsUTF8(arg);
    if(NULL == path) {
        return NULL;
    }
    if(0 > SDL_SaveBMP(self->surface, path)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_SavePNG(PySDL_Surface *self, PyObject *arg) {
    const char *path = PyUnicode_AsUTF8(arg);
    if(NULL == path) {
        return NULL;
    }
    if(0 > IMG_SavePNG(self->surface, path)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_SaveJPG(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    const char *path;
    int quality = 90;

    static char *kwlist[] = {"path", "quality", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|i", kwlist, &path, &quality)) {
        return NULL;
    }
    if(0 > IMG_SaveJPG(self->surface, path, quality)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

//=========================================================
// blitting
//=========================================================

// Shared parser for the (src, srcrect, dstrect) blit signature.
static int _blit_args(PyObject *args, PyObject *kwds, PySDL_Surface **src,
                      SDL_Rect *srcrect, SDL_Rect **sr, SDL_Rect *dstrect, SDL_Rect **dr) {
    PyObject *srcrect_py = NULL;
    PyObject *dstrect_py = NULL;
    static char *kwlist[] = {"src", "srcrect", "dstrect", NULL};

    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O!|OO", kwlist,
        &PySDL_Surface_Type, src, &srcrect_py, &dstrect_py)) {
        return 0;
    }

    *sr = NULL;
    *dr = NULL;
    if(srcrect_py && srcrect_py != Py_None) {
        if(!PyToRect(srcrect_py, srcrect)) {
            return 0;
        }
        *sr = srcrect;
    }
    if(dstrect_py && dstrect_py != Py_None) {
        if(!PyToRect(dstrect_py, dstrect)) {
            return 0;
        }
        *dr = dstrect;
    }
    return 1;
}

static PyObject * PySDL_Surface_Blit(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    PySDL_Surface *src;
    SDL_Rect srcrect, dstrect;
    SDL_Rect *sr, *dr;

    if(!_blit_args(args, kwds, &src, &srcrect, &sr, &dstrect, &dr)) {
        return NULL;
    }
    if(0 > SDL_BlitSurface(src->surface, sr, self->surface, dr)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_BlitScaled(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    PySDL_Surface *src;
    SDL_Rect srcrect, dstrect;
    SDL_Rect *sr, *dr;

    if(!_blit_args(args, kwds, &src, &srcrect, &sr, &dstrect, &dr)) {
        return NULL;
    }
    if(0 > SDL_BlitScaled(src->surface, sr, self->surface, dr)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_SoftStretch(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    PySDL_Surface *src;
    SDL_Rect srcrect, dstrect;
    SDL_Rect *sr, *dr;

    if(!_blit_args(args, kwds, &src, &srcrect, &sr, &dstrect, &dr)) {
        return NULL;
    }
    if(0 > SDL_SoftStretch(src->surface, sr, self->surface, dr)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_FillRect(PySDL_Surface *self, PyObject *args) {
    PyObject *rect_py;
    PyObject *color_py;
    if(!PyArg_ParseTuple(args, "OO", &rect_py, &color_py)) {
        return NULL;
    }

    SDL_Rect rect;
    SDL_Rect *rp = NULL;
    if(rect_py != Py_None) {
        if(!PyToRect(rect_py, &rect)) {
            return NULL;
        }
        rp = &rect;
    }

    Uint32 color;
    if(!PyToPixel(color_py, self->surface->format, &color)) {
        return NULL;
    }
    if(0 > SDL_FillRect(self->surface, rp, color)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_FillRects(PySDL_Surface *self, PyObject *args) {
    PyObject *rects_py;
    PyObject *color_py;
    if(!PyArg_ParseTuple(args, "OO", &rects_py, &color_py)) {
        return NULL;
    }

    PyObject *fast = PySequence_Fast(rects_py, "expected a list of (x, y, w, h) rects");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    SDL_Rect *rects = PyMem_New(SDL_Rect, n > 0 ? n : 1);
    if(NULL == rects) {
        Py_DECREF(fast);
        return PyErr_NoMemory();
    }
    for(Py_ssize_t idx = 0; idx < n; ++idx) {
        if(!PyToRect(PySequence_Fast_GET_ITEM(fast, idx), &rects[idx])) {
            PyMem_Free(rects);
            Py_DECREF(fast);
            return NULL;
        }
    }
    Py_DECREF(fast);

    Uint32 color;
    if(!PyToPixel(color_py, self->surface->format, &color)) {
        PyMem_Free(rects);
        return NULL;
    }

    int rc = SDL_FillRects(self->surface, rects, (int)n, color);
    PyMem_Free(rects);
    if(0 > rc) {
        return _raise();
    }
    Py_RETURN_NONE;
}

//=========================================================
// pixel format
//=========================================================

static PyObject * PySDL_Surface_MapRGB(PySDL_Surface *self, PyObject *arg) {
    SDL_Color c;
    if(!PyToColor(arg, &c)) {
        return NULL;
    }
    return PyLong_FromUnsignedLong(SDL_MapRGB(self->surface->format, c.r, c.g, c.b));
}

static PyObject * PySDL_Surface_MapRGBA(PySDL_Surface *self, PyObject *arg) {
    SDL_Color c;
    if(!PyToColor(arg, &c)) {
        return NULL;
    }
    return PyLong_FromUnsignedLong(SDL_MapRGBA(self->surface->format, c.r, c.g, c.b, c.a));
}

static PyObject * PySDL_Surface_GetPixelFormat(PySDL_Surface *self, PyObject *ign) {
    // A standalone copy of the format enum (no shared palette).
    SDL_PixelFormat *format = SDL_AllocFormat(self->surface->format->format);
    if(NULL == format) {
        return _raise();
    }
    PySDL_PixelFormat *wrapper = (PySDL_PixelFormat *)PySDL_New(&PySDL_PixelFormat_Type);
    if(NULL == wrapper) {
        SDL_FreeFormat(format);
        return NULL;
    }
    wrapper->format = format;
    return (PyObject *)wrapper;
}

//=========================================================
// surface state
//=========================================================

static PyObject * PySDL_Surface_SetColorKey(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    PyObject *key_py;
    int enable = 1;

    static char *kwlist[] = {"key", "enable", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|p", kwlist, &key_py, &enable)) {
        return NULL;
    }

    Uint32 key;
    if(!PyToPixel(key_py, self->surface->format, &key)) {
        return NULL;
    }
    if(0 > SDL_SetColorKey(self->surface, enable ? true : false, key)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_GetColorKey(PySDL_Surface *self, PyObject *ign) {
    Uint32 key = 0;
    if(0 > SDL_GetColorKey(self->surface, &key)) {
        SDL_ClearError();  // "surface doesn't have a colorkey" is not an error here
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(key);
}

static PyObject * PySDL_Surface_SetBlendMode(PySDL_Surface *self, PyObject *arg) {
    long mode = PyLong_AsLong(arg);
    if(-1 == mode && PyErr_Occurred()) {
        return NULL;
    }
    if(0 > SDL_SetSurfaceBlendMode(self->surface, (SDL_BlendMode)mode)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_GetBlendMode(PySDL_Surface *self, PyObject *ign) {
    SDL_BlendMode mode = SDL_BLENDMODE_NONE;
    if(0 > SDL_GetSurfaceBlendMode(self->surface, &mode)) {
        return _raise();
    }
    return PyLong_FromLong(mode);
}

static PyObject * PySDL_Surface_SetColorMod(PySDL_Surface *self, PyObject *args) {
    unsigned char r, g, b;
    if(!PyArg_ParseTuple(args, "bbb", &r, &g, &b)) {
        return NULL;
    }
    if(0 > SDL_SetSurfaceColorMod(self->surface, r, g, b)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_GetColorMod(PySDL_Surface *self, PyObject *ign) {
    Uint8 r = 0, g = 0, b = 0;
    if(0 > SDL_GetSurfaceColorMod(self->surface, &r, &g, &b)) {
        return _raise();
    }
    return Py_BuildValue("(iii)", r, g, b);
}

static PyObject * PySDL_Surface_SetAlphaMod(PySDL_Surface *self, PyObject *arg) {
    long a = PyLong_AsLong(arg);
    if(-1 == a && PyErr_Occurred()) {
        return NULL;
    }
    if(0 > SDL_SetSurfaceAlphaMod(self->surface, (Uint8)a)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_GetAlphaMod(PySDL_Surface *self, PyObject *ign) {
    Uint8 a = 0;
    if(0 > SDL_GetSurfaceAlphaMod(self->surface, &a)) {
        return _raise();
    }
    return PyLong_FromLong(a);
}

static PyObject * PySDL_Surface_SetClipRect(PySDL_Surface *self, PyObject *arg) {
    SDL_Rect rect;
    SDL_Rect *rp = NULL;
    if(arg != Py_None) {
        if(!PyToRect(arg, &rect)) {
            return NULL;
        }
        rp = &rect;
    }
    return PyBool_FromLong(SDL_SetClipRect(self->surface, rp));
}

static PyObject * PySDL_Surface_GetClipRect(PySDL_Surface *self, PyObject *ign) {
    SDL_Rect rect;
    SDL_GetClipRect(self->surface, &rect);
    return RectToPy(&rect);
}

static PyObject * PySDL_Surface_SetRLE(PySDL_Surface *self, PyObject *arg) {
    int enable = PyObject_IsTrue(arg);
    if(-1 == enable) {
        return NULL;
    }
    if(0 > SDL_SetSurfaceRLE(self->surface, enable)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_SetPalette(PySDL_Surface *self, PyObject *arg) {
    if(!PyObject_TypeCheck(arg, &PySDL_Palette_Type)) {
        PyErr_SetString(PyExc_TypeError, "expected an SDL2.Palette");
        return NULL;
    }
    if(0 > SDL_SetSurfacePalette(self->surface, ((PySDL_Palette *)arg)->palette)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

//=========================================================
// conversion
//=========================================================

static PyObject * PySDL_Surface_Convert(PySDL_Surface *self, PyObject *args) {
    PyObject *format_py;
    unsigned int flags = 0;
    if(!PyArg_ParseTuple(args, "O|I", &format_py, &flags)) {
        return NULL;
    }

    SDL_Surface *converted = NULL;
    if(PyObject_TypeCheck(format_py, &PySDL_PixelFormat_Type)) {
        converted = SDL_ConvertSurface(self->surface, ((PySDL_PixelFormat *)format_py)->format, flags);
    } else if(PyLong_Check(format_py)) {
        converted = SDL_ConvertSurfaceFormat(self->surface, (Uint32)PyLong_AsUnsignedLong(format_py), flags);
    } else {
        PyErr_SetString(PyExc_TypeError, "expected a pixel-format enum int or SDL2.PixelFormat");
        return NULL;
    }
    return _wrap(converted);
}

static PyObject * PySDL_Surface_Duplicate(PySDL_Surface *self, PyObject *ign) {
    return _wrap(SDL_DuplicateSurface(self->surface));
}
