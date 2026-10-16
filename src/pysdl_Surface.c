#include "pysdl.h"

static int        PySDL_Surface_Type_init    (PySDL_Surface*, PyObject*, PyObject*);
static void       PySDL_Surface_Type_dealloc (PySDL_Surface*);

static PyObject * PySDL_Surface_LockSurface   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_UnlockSurface (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SaveBMP       (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_SavePNG       (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_SaveJPG       (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_SaveAVIF      (PySDL_Surface*, PyObject*, PyObject*);
#if SDL_IMAGE_VERSION_ATLEAST(3,4,0)
static PyObject * PySDL_Surface_SaveGIF       (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_SaveTGA       (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_SaveICO       (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_SaveCUR       (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_SaveWEBP      (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_Save          (PySDL_Surface*, PyObject*, PyObject*);
#endif

static PyObject * PySDL_Surface_Blit          (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_BlitScaled    (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_SoftStretch   (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_SoftStretchLinear (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_Stretch       (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_FillRect      (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_FillRects     (PySDL_Surface*, PyObject*);

static PyObject * PySDL_Surface_MapRGB        (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_MapRGBA       (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_GetPixelFormat(PySDL_Surface*, PyObject*);

static PyObject * PySDL_Surface_SetColorKey   (PySDL_Surface*, PyObject*, PyObject*);
static PyObject * PySDL_Surface_GetColorKey   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_HasColorKey   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetBlendMode  (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_GetBlendMode  (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetColorMod   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_GetColorMod   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetAlphaMod   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_GetAlphaMod   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetClipRect   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_GetClipRect   (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetRLE        (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_HasRLE        (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_SetPalette    (PySDL_Surface*, PyObject*);

static PyObject * PySDL_Surface_Convert       (PySDL_Surface*, PyObject*);
static PyObject * PySDL_Surface_Duplicate     (PySDL_Surface*, PyObject*);

static PyMethodDef PySDL_Surface_methods[] = {
    { "LockSurface",    (PyCFunction)PySDL_Surface_LockSurface,    METH_NOARGS  },
    { "UnlockSurface",  (PyCFunction)PySDL_Surface_UnlockSurface,  METH_NOARGS  },
    { "SaveBMP",        (PyCFunction)PySDL_Surface_SaveBMP,        METH_VARARGS | METH_KEYWORDS },
    { "SavePNG",        (PyCFunction)PySDL_Surface_SavePNG,        METH_VARARGS | METH_KEYWORDS },
    { "SaveJPG",        (PyCFunction)PySDL_Surface_SaveJPG,        METH_VARARGS | METH_KEYWORDS },
    { "SaveAVIF",       (PyCFunction)PySDL_Surface_SaveAVIF,       METH_VARARGS | METH_KEYWORDS },
#if SDL_IMAGE_VERSION_ATLEAST(3,4,0)
    { "SaveGIF",        (PyCFunction)PySDL_Surface_SaveGIF,        METH_VARARGS | METH_KEYWORDS },
    { "SaveTGA",        (PyCFunction)PySDL_Surface_SaveTGA,        METH_VARARGS | METH_KEYWORDS },
    { "SaveICO",        (PyCFunction)PySDL_Surface_SaveICO,        METH_VARARGS | METH_KEYWORDS },
    { "SaveCUR",        (PyCFunction)PySDL_Surface_SaveCUR,        METH_VARARGS | METH_KEYWORDS },
    { "SaveWEBP",       (PyCFunction)PySDL_Surface_SaveWEBP,       METH_VARARGS | METH_KEYWORDS },
    { "Save",           (PyCFunction)PySDL_Surface_Save,           METH_VARARGS | METH_KEYWORDS },
#endif

    { "Blit",           (PyCFunction)PySDL_Surface_Blit,           METH_VARARGS | METH_KEYWORDS },
    { "BlitScaled",     (PyCFunction)PySDL_Surface_BlitScaled,     METH_VARARGS | METH_KEYWORDS },
    { "SoftStretch",    (PyCFunction)PySDL_Surface_SoftStretch,    METH_VARARGS | METH_KEYWORDS },
    { "SoftStretchLinear", (PyCFunction)PySDL_Surface_SoftStretchLinear, METH_VARARGS | METH_KEYWORDS },
    { "Stretch",        (PyCFunction)PySDL_Surface_Stretch,        METH_VARARGS | METH_KEYWORDS },
    { "FillRect",       (PyCFunction)PySDL_Surface_FillRect,       METH_VARARGS },
    { "FillRects",      (PyCFunction)PySDL_Surface_FillRects,      METH_VARARGS },

    { "MapRGB",         (PyCFunction)PySDL_Surface_MapRGB,         METH_O       },
    { "MapRGBA",        (PyCFunction)PySDL_Surface_MapRGBA,        METH_O       },
    { "GetPixelFormat", (PyCFunction)PySDL_Surface_GetPixelFormat, METH_NOARGS  },

    { "SetColorKey",    (PyCFunction)PySDL_Surface_SetColorKey,    METH_VARARGS | METH_KEYWORDS },
    { "GetColorKey",    (PyCFunction)PySDL_Surface_GetColorKey,    METH_NOARGS  },
    { "HasColorKey",    (PyCFunction)PySDL_Surface_HasColorKey,    METH_NOARGS  },
    { "SetBlendMode",   (PyCFunction)PySDL_Surface_SetBlendMode,   METH_O       },
    { "GetBlendMode",   (PyCFunction)PySDL_Surface_GetBlendMode,   METH_NOARGS  },
    { "SetColorMod",    (PyCFunction)PySDL_Surface_SetColorMod,    METH_VARARGS },
    { "GetColorMod",    (PyCFunction)PySDL_Surface_GetColorMod,    METH_NOARGS  },
    { "SetAlphaMod",    (PyCFunction)PySDL_Surface_SetAlphaMod,    METH_O       },
    { "GetAlphaMod",    (PyCFunction)PySDL_Surface_GetAlphaMod,    METH_NOARGS  },
    { "SetClipRect",    (PyCFunction)PySDL_Surface_SetClipRect,    METH_O       },
    { "GetClipRect",    (PyCFunction)PySDL_Surface_GetClipRect,    METH_NOARGS  },
    { "SetRLE",         (PyCFunction)PySDL_Surface_SetRLE,         METH_O       },
    { "HasRLE",         (PyCFunction)PySDL_Surface_HasRLE,         METH_NOARGS  },
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
    self->window_id = 0;
    return 0;
}

void PySDL_SurfaceDetach(PySDL_Surface *self) {
    // The old surface is SDL's to free (or already freed): don't touch it.
    self->surface = SDL_CreateSurface(0, 0, SDL_PIXELFORMAT_RGBA32);
    self->shouldFree = 1;
    self->window_id = 0;
}

static void PySDL_Surface_Type_dealloc(PySDL_Surface *self) {
    if(0 != self->window_id) {
        PySDL_WindowSurfaceForget(self);
    }
    if(NULL != self->surface) {
        if(self->shouldFree) {
            SDL_DestroySurface(self->surface);
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
        SDL_DestroySurface(surface);
        return NULL;
    }
    wrapper->surface = surface;
    return (PyObject *)wrapper;
}

// SDL3: the surface carries the format enum; details / palette are looked up.
// NULL (SDL's error set) for an uninitialised Surface.
static const SDL_PixelFormatDetails * _details(SDL_Surface *s) {
    if(NULL == s) {
        SDL_InvalidParamError("surface");
        return NULL;
    }
    return SDL_GetPixelFormatDetails(s->format);
}

//=========================================================
// attributes
//=========================================================

static PyObject * PySDL_Surface_getter(PyObject *self, void *param) {
    SDL_Surface *surface = ((PySDL_Surface *)self)->surface;
    if(NULL == surface) {
        PyErr_SetString(pysdl_Error, "Surface is not initialised");
        return NULL;
    }

    switch((long)param) {
    case PySDL_SURFACE_WIDTH:  return PyLong_FromLong(surface->w);
    case PySDL_SURFACE_HEIGHT: return PyLong_FromLong(surface->h);
    case PySDL_SURFACE_BPP:    return PyLong_FromLong(SDL_BITSPERPIXEL(surface->format));
    case PySDL_SURFACE_PITCH:  return PyLong_FromLong(surface->pitch);
    case PySDL_SURFACE_FORMAT: return PyLong_FromUnsignedLong(surface->format);
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
    if(NULL == surface) {
        PyErr_SetString(pysdl_Error, "Surface is not initialised");
        return -1;
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
    if(!SDL_LockSurface(self->surface)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_UnlockSurface(PySDL_Surface *self, PyObject *ign) {
    SDL_UnlockSurface(self->surface);
    Py_RETURN_NONE;
}

// SaveBMP / SavePNG / SaveJPG / ... (path=None[, quality]): write the file at
// `path` (str / os.PathLike), or with no path return the encoded image as
// bytes. SDL writes into a file stream or a growable in-memory one
// (PySDL_SaveOpen / PySDL_SaveClose); the GIL is held while it encodes so
// nothing can free the surface under it. SDL_image 3.4 adds GIF / TGA / ICO /
// CUR / WEBP and Save(path=None, type=None), which picks the format by name.
enum { SAVE_BMP, SAVE_PNG, SAVE_JPG, SAVE_AVIF, SAVE_GIF, SAVE_TGA, SAVE_ICO, SAVE_CUR, SAVE_WEBP, SAVE_TYPED };

static PyObject * _save(PySDL_Surface *self, PyObject *dest, int kind, double quality, const char *type) {
    SDL_Surface *s = self->surface;
    if(NULL == s) {
        PyErr_SetString(pysdl_Error, "Surface is not initialised");
        return NULL;
    }
    // SDL_image 3.4 checks this itself; 3.2's PNG writer "succeeds" writing nothing.
    if(SDL_ISPIXELFORMAT_INDEXED(s->format) && NULL == SDL_GetSurfacePalette(s)) {
        PyErr_SetString(pysdl_Error, "Indexed surfaces must have a palette");
        return NULL;
    }
    PyObject *path;
    SDL_IOStream *io = PySDL_SaveOpen(dest, &path);
    if(NULL == io) {
        return NULL;
    }

    bool ok = false;
    switch(kind) {
    case SAVE_BMP:   ok = SDL_SaveBMP_IO(s, io, false);                    break;
    case SAVE_PNG:   ok = IMG_SavePNG_IO(s, io, false);                    break;
    case SAVE_JPG:   ok = IMG_SaveJPG_IO(s, io, false, (int)quality);      break;
    case SAVE_AVIF:  ok = IMG_SaveAVIF_IO(s, io, false, (int)quality);     break;
#if SDL_IMAGE_VERSION_ATLEAST(3,4,0)
    case SAVE_GIF:   ok = IMG_SaveGIF_IO(s, io, false);                    break;
    case SAVE_TGA:   ok = IMG_SaveTGA_IO(s, io, false);                    break;
    case SAVE_ICO:   ok = IMG_SaveICO_IO(s, io, false);                    break;
    case SAVE_CUR:   ok = IMG_SaveCUR_IO(s, io, false);                    break;
    case SAVE_WEBP:  ok = IMG_SaveWEBP_IO(s, io, false, (float)quality);   break;
    case SAVE_TYPED: ok = IMG_SaveTyped_IO(s, io, false, type);            break;
#endif
    }
    return PySDL_SaveClose(io, path, ok);
}

// (path=None)
static PyObject * _save_plain(PySDL_Surface *self, PyObject *args, PyObject *kwds, int kind) {
    PyObject *dest = Py_None;
    static char *kwlist[] = {"path", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|O", kwlist, &dest)) {
        return NULL;
    }
    return _save(self, dest, kind, 0, NULL);
}

// (path=None, quality=90): an int 0-100, a float for WEBP
static PyObject * _save_quality(PySDL_Surface *self, PyObject *args, PyObject *kwds, int kind) {
    PyObject *dest = Py_None;
    int quality = 90;
    float fquality = 90.0f;
    static char *kwlist[] = {"path", "quality", NULL};
    if(SAVE_WEBP == kind
        ? !PyArg_ParseTupleAndKeywords(args, kwds, "|Of", kwlist, &dest, &fquality)
        : !PyArg_ParseTupleAndKeywords(args, kwds, "|Oi", kwlist, &dest, &quality)) {
        return NULL;
    }
    return _save(self, dest, kind, (SAVE_WEBP == kind) ? fquality : quality, NULL);
}

static PyObject * PySDL_Surface_SaveBMP(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _save_plain(self, args, kwds, SAVE_BMP);
}

static PyObject * PySDL_Surface_SavePNG(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _save_plain(self, args, kwds, SAVE_PNG);
}

static PyObject * PySDL_Surface_SaveJPG(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _save_quality(self, args, kwds, SAVE_JPG);
}

static PyObject * PySDL_Surface_SaveAVIF(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _save_quality(self, args, kwds, SAVE_AVIF);
}

#if SDL_IMAGE_VERSION_ATLEAST(3,4,0)
static PyObject * PySDL_Surface_SaveGIF(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _save_plain(self, args, kwds, SAVE_GIF);
}

static PyObject * PySDL_Surface_SaveTGA(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _save_plain(self, args, kwds, SAVE_TGA);
}

static PyObject * PySDL_Surface_SaveICO(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _save_plain(self, args, kwds, SAVE_ICO);
}

static PyObject * PySDL_Surface_SaveCUR(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _save_plain(self, args, kwds, SAVE_CUR);
}

static PyObject * PySDL_Surface_SaveWEBP(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _save_quality(self, args, kwds, SAVE_WEBP);
}

// Save(path=None, type=None): `type` ("PNG", "JPG", "WEBP", ...) defaults to
// the path's extension, as IMG_Save does; quality-taking formats use 90.
static PyObject * PySDL_Surface_Save(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    PyObject *dest = Py_None;
    const char *type = NULL;
    static char *kwlist[] = {"path", "type", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|Oz", kwlist, &dest, &type)) {
        return NULL;
    }
    if(NULL != type) {
        return _save(self, dest, SAVE_TYPED, 0, type);
    }
    if(Py_None == dest) {
        PyErr_SetString(PyExc_TypeError, "Save() needs a type when no path is given");
        return NULL;
    }

    PyObject *path;
    int is_path = PySDL_PathFromObject(dest, &path);
    if(0 >= is_path) {
        if(0 == is_path) {
            PyErr_SetString(PyExc_TypeError, "path must be str, os.PathLike or None");
        }
        return NULL;
    }
    PyObject *result = NULL;
    const char *file = PyUnicode_AsUTF8(path);
    const char *ext = file ? SDL_strrchr(file, '.') : NULL;
    if(file && !ext) {
        PyErr_SetString(PyExc_ValueError, "the path has no extension: pass type");
    } else if(file) {
        result = _save(self, path, SAVE_TYPED, 0, ext + 1);  // `path` keeps `ext` alive
    }
    Py_DECREF(path);
    return result;
}
#endif

//=========================================================
// blitting
//=========================================================

// Shared parser for the (src, srcrect, dstrect) blit signature, plus a
// trailing scale_mode when `mode` is given.
static int _blit_args(PyObject *args, PyObject *kwds, PySDL_Surface **src,
                      SDL_Rect *srcrect, SDL_Rect **sr, SDL_Rect *dstrect, SDL_Rect **dr, int *mode) {
    PyObject *srcrect_py = NULL;
    PyObject *dstrect_py = NULL;
    static char *kwlist[] = {"src", "srcrect", "dstrect", NULL};
    static char *kwlist_mode[] = {"src", "srcrect", "dstrect", "scale_mode", NULL};

    if(mode ? !PyArg_ParseTupleAndKeywords(args, kwds, "O!|OOi", kwlist_mode,
                  &PySDL_Surface_Type, src, &srcrect_py, &dstrect_py, mode)
            : !PyArg_ParseTupleAndKeywords(args, kwds, "O!|OO", kwlist,
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

    if(!_blit_args(args, kwds, &src, &srcrect, &sr, &dstrect, &dr, NULL)) {
        return NULL;
    }
    if(!SDL_BlitSurface(src->surface, sr, self->surface, dr)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_BlitScaled(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    PySDL_Surface *src;
    SDL_Rect srcrect, dstrect;
    SDL_Rect *sr, *dr;

    if(!_blit_args(args, kwds, &src, &srcrect, &sr, &dstrect, &dr, NULL)) {
        return NULL;
    }
    if(!SDL_BlitSurfaceScaled(src->surface, sr, self->surface, dr, SDL_SCALEMODE_LINEAR)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

// SoftStretch(src, srcrect=None, dstrect=None) (nearest), SoftStretchLinear
// (bilinear: 32-bit formats only) and Stretch(..., scale_mode=SCALEMODE_NEAREST),
// all SDL_StretchSurface. SDL trusts the rect sizes - a zero or negative width
// reads outside the source rows (a 2-item rect from PyToRect has w = h = -1) -
// and writes a dstrect on a YUV surface unclipped, so sizes must not be
// negative (zero copies nothing) and a YUV destination is stretched to whole.
static PyObject * _stretch(PySDL_Surface *self, PyObject *args, PyObject *kwds, int mode, int has_mode) {
    PySDL_Surface *src;
    SDL_Rect srcrect, dstrect;
    SDL_Rect *sr, *dr;

    if(!_blit_args(args, kwds, &src, &srcrect, &sr, &dstrect, &dr, has_mode ? &mode : NULL)) {
        return NULL;
    }
    SDL_Rect *rects[2] = { sr, dr };
    for(int idx = 0; idx < 2; ++idx) {
        if(NULL != rects[idx] && (rects[idx]->w < 0 || rects[idx]->h < 0)) {
            PyErr_SetString(PyExc_ValueError, "stretch rects are (x, y, w, h) with w, h >= 0");
            return NULL;
        }
    }
    for(int idx = 0; idx < 2; ++idx) {
        if(NULL != rects[idx] && (0 == rects[idx]->w || 0 == rects[idx]->h)) {
            Py_RETURN_NONE;  // nothing to copy
        }
    }
    SDL_Surface *dst = self->surface;
    if(NULL != dst && NULL != dr && SDL_ISPIXELFORMAT_FOURCC(dst->format)
        && (dr->x || dr->y || dr->w != dst->w || dr->h != dst->h)) {
        PyErr_SetString(PyExc_ValueError, "a YUV surface can only be stretched to as a whole (dstrect=None)");
        return NULL;
    }
    if(!SDL_StretchSurface(src->surface, sr, dst, dr, (SDL_ScaleMode)mode)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_SoftStretch(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _stretch(self, args, kwds, SDL_SCALEMODE_NEAREST, 0);
}

static PyObject * PySDL_Surface_SoftStretchLinear(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _stretch(self, args, kwds, SDL_SCALEMODE_LINEAR, 0);
}

static PyObject * PySDL_Surface_Stretch(PySDL_Surface *self, PyObject *args, PyObject *kwds) {
    return _stretch(self, args, kwds, SDL_SCALEMODE_NEAREST, 1);
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
    if(!PyToPixel(color_py, _details(self->surface), SDL_GetSurfacePalette(self->surface), &color)) {
        return NULL;
    }
    if(!SDL_FillSurfaceRect(self->surface, rp, color)) {
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
    if(!PyToPixel(color_py, _details(self->surface), SDL_GetSurfacePalette(self->surface), &color)) {
        PyMem_Free(rects);
        return NULL;
    }

    bool ok = SDL_FillSurfaceRects(self->surface, rects, (int)n, color);
    PyMem_Free(rects);
    if(!ok) {
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
    return PyLong_FromUnsignedLong(SDL_MapSurfaceRGB(self->surface, c.r, c.g, c.b));
}

static PyObject * PySDL_Surface_MapRGBA(PySDL_Surface *self, PyObject *arg) {
    SDL_Color c;
    if(!PyToColor(arg, &c)) {
        return NULL;
    }
    return PyLong_FromUnsignedLong(SDL_MapSurfaceRGBA(self->surface, c.r, c.g, c.b, c.a));
}

static PyObject * PySDL_Surface_GetPixelFormat(PySDL_Surface *self, PyObject *ign) {
    const SDL_PixelFormatDetails *details = _details(self->surface);
    if(NULL == details) {
        return _raise();
    }
    PySDL_PixelFormat *wrapper = (PySDL_PixelFormat *)PySDL_New(&PySDL_PixelFormat_Type);
    if(NULL == wrapper) {
        return NULL;
    }
    wrapper->format  = self->surface->format;
    wrapper->details = details;   // SDL-cached, never freed
    wrapper->palette = NULL;
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
    if(!PyToPixel(key_py, _details(self->surface), SDL_GetSurfacePalette(self->surface), &key)) {
        return NULL;
    }
    if(!SDL_SetSurfaceColorKey(self->surface, enable ? true : false, key)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_GetColorKey(PySDL_Surface *self, PyObject *ign) {
    Uint32 key = 0;
    if(!SDL_GetSurfaceColorKey(self->surface, &key)) {
        SDL_ClearError();  // "surface doesn't have a colorkey" is not an error here
        Py_RETURN_NONE;
    }
    return PyLong_FromUnsignedLong(key);
}

static PyObject * PySDL_Surface_HasColorKey(PySDL_Surface *self, PyObject *ign) {
    return PyBool_FromLong(SDL_SurfaceHasColorKey(self->surface));
}

static PyObject * PySDL_Surface_SetBlendMode(PySDL_Surface *self, PyObject *arg) {
    long mode = PyLong_AsLong(arg);
    if(-1 == mode && PyErr_Occurred()) {
        return NULL;
    }
    if(!SDL_SetSurfaceBlendMode(self->surface, (SDL_BlendMode)mode)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_GetBlendMode(PySDL_Surface *self, PyObject *ign) {
    SDL_BlendMode mode = SDL_BLENDMODE_NONE;
    if(!SDL_GetSurfaceBlendMode(self->surface, &mode)) {
        return _raise();
    }
    return PyLong_FromLong(mode);
}

static PyObject * PySDL_Surface_SetColorMod(PySDL_Surface *self, PyObject *args) {
    unsigned char r, g, b;
    if(!PyArg_ParseTuple(args, "bbb", &r, &g, &b)) {
        return NULL;
    }
    if(!SDL_SetSurfaceColorMod(self->surface, r, g, b)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_GetColorMod(PySDL_Surface *self, PyObject *ign) {
    Uint8 r = 0, g = 0, b = 0;
    if(!SDL_GetSurfaceColorMod(self->surface, &r, &g, &b)) {
        return _raise();
    }
    return Py_BuildValue("(iii)", r, g, b);
}

static PyObject * PySDL_Surface_SetAlphaMod(PySDL_Surface *self, PyObject *arg) {
    long a = PyLong_AsLong(arg);
    if(-1 == a && PyErr_Occurred()) {
        return NULL;
    }
    if(!SDL_SetSurfaceAlphaMod(self->surface, (Uint8)a)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_GetAlphaMod(PySDL_Surface *self, PyObject *ign) {
    Uint8 a = 0;
    if(!SDL_GetSurfaceAlphaMod(self->surface, &a)) {
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
    return PyBool_FromLong(SDL_SetSurfaceClipRect(self->surface, rp));
}

static PyObject * PySDL_Surface_GetClipRect(PySDL_Surface *self, PyObject *ign) {
    SDL_Rect rect;
    SDL_GetSurfaceClipRect(self->surface, &rect);
    return RectToPy(&rect);
}

static PyObject * PySDL_Surface_SetRLE(PySDL_Surface *self, PyObject *arg) {
    int enable = PyObject_IsTrue(arg);
    if(-1 == enable) {
        return NULL;
    }
    if(!SDL_SetSurfaceRLE(self->surface, enable)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Surface_HasRLE(PySDL_Surface *self, PyObject *ign) {
    return PyBool_FromLong(SDL_SurfaceHasRLE(self->surface));
}

static PyObject * PySDL_Surface_SetPalette(PySDL_Surface *self, PyObject *arg) {
    if(!PyObject_TypeCheck(arg, &PySDL_Palette_Type)) {
        PyErr_SetString(PyExc_TypeError, "expected an SDL3.Palette");
        return NULL;
    }
    if(!SDL_SetSurfacePalette(self->surface, ((PySDL_Palette *)arg)->palette)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

//=========================================================
// conversion
//=========================================================

static PyObject * PySDL_Surface_Convert(PySDL_Surface *self, PyObject *args) {
    PyObject *format_py;
    if(!PyArg_ParseTuple(args, "O", &format_py)) {
        return NULL;
    }

    SDL_PixelFormat format;
    if(PyObject_TypeCheck(format_py, &PySDL_PixelFormat_Type)) {
        format = ((PySDL_PixelFormat *)format_py)->format;
    } else if(PyLong_Check(format_py)) {
        format = (SDL_PixelFormat)PyLong_AsUnsignedLong(format_py);
    } else {
        PyErr_SetString(PyExc_TypeError, "expected a pixel-format enum int or SDL3.PixelFormat");
        return NULL;
    }
    return _wrap(SDL_ConvertSurface(self->surface, format));
}

static PyObject * PySDL_Surface_Duplicate(PySDL_Surface *self, PyObject *ign) {
    return _wrap(SDL_DuplicateSurface(self->surface));
}
