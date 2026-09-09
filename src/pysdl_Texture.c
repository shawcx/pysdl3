#include "pysdl.h"

static int        PySDL_Texture_Type_init    (PySDL_Texture*, PyObject*, PyObject*);
static void       PySDL_Texture_Type_dealloc (PySDL_Texture*);

static PyObject * PySDL_Texture_Query        (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_Update       (PySDL_Texture*, PyObject*, PyObject*);
static PyObject * PySDL_Texture_UpdateYUV    (PySDL_Texture*, PyObject*, PyObject*);
static PyObject * PySDL_Texture_Lock         (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_Unlock       (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_SetColorMod  (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_GetColorMod  (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_SetAlphaMod  (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_GetAlphaMod  (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_SetBlendMode (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_GetBlendMode (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_SetScaleMode (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_GetScaleMode (PySDL_Texture*, PyObject*);

static PyMethodDef PySDL_Texture_methods[] = {
    { "Query",        (PyCFunction)PySDL_Texture_Query,        METH_NOARGS  },
    { "Update",       (PyCFunction)PySDL_Texture_Update,       METH_VARARGS | METH_KEYWORDS },
    { "UpdateYUV",    (PyCFunction)PySDL_Texture_UpdateYUV,    METH_VARARGS | METH_KEYWORDS },
    { "Lock",         (PyCFunction)PySDL_Texture_Lock,         METH_VARARGS },
    { "Unlock",       (PyCFunction)PySDL_Texture_Unlock,       METH_NOARGS  },
    { "SetColorMod",  (PyCFunction)PySDL_Texture_SetColorMod,  METH_VARARGS },
    { "GetColorMod",  (PyCFunction)PySDL_Texture_GetColorMod,  METH_NOARGS  },
    { "SetAlphaMod",  (PyCFunction)PySDL_Texture_SetAlphaMod,  METH_O       },
    { "GetAlphaMod",  (PyCFunction)PySDL_Texture_GetAlphaMod,  METH_NOARGS  },
    { "SetBlendMode", (PyCFunction)PySDL_Texture_SetBlendMode, METH_O       },
    { "GetBlendMode", (PyCFunction)PySDL_Texture_GetBlendMode, METH_NOARGS  },
    { "SetScaleMode", (PyCFunction)PySDL_Texture_SetScaleMode, METH_O       },
    { "GetScaleMode", (PyCFunction)PySDL_Texture_GetScaleMode, METH_NOARGS  },
    { NULL }
};

PyTypeObject PySDL_Texture_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Texture",
    .tp_basicsize = sizeof(PySDL_Texture),
    .tp_dealloc   = (destructor)PySDL_Texture_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Texture(renderer, format=PIXELFORMAT_RGBA8888, access=TEXTUREACCESS_STATIC, size=(0,0))",
    .tp_methods   = PySDL_Texture_methods,
    .tp_init      = (initproc)PySDL_Texture_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_Texture_Type_init(PySDL_Texture *self, PyObject *args, PyObject *kwds) {
    PyObject *renderer = NULL;
    unsigned int format = SDL_PIXELFORMAT_RGBA8888;
    int access = SDL_TEXTUREACCESS_STATIC;
    int w = 0;
    int h = 0;

    static char *kwlist[] = {"renderer", "format", "access", "size", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|OIi(ii)", kwlist,
        &renderer, &format, &access, &w, &h)) {
        return -1;
    }

    self->texture = NULL;

    // No renderer: internal allocation; the caller fills in ->texture.
    if(renderer && renderer != Py_None) {
        if(!PyObject_TypeCheck(renderer, &PySDL_Renderer_Type)) {
            PyErr_SetString(PyExc_TypeError, "renderer must be an SDL3.Renderer");
            return -1;
        }
        self->texture = SDL_CreateTexture(((PySDL_Renderer *)renderer)->renderer,
            (SDL_PixelFormat)format, (SDL_TextureAccess)access, w, h);
        if(NULL == self->texture) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }

    return 0;
}

static void PySDL_Texture_Type_dealloc(PySDL_Texture *self) {
    if(NULL != self->texture) {
        SDL_DestroyTexture(self->texture);
        self->texture = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * _raise(void) {
    PyErr_SetString(pysdl_Error, SDL_GetError());
    return NULL;
}

// SDL3 dropped SDL_QueryTexture: format / access come from the texture's
// properties, the size from SDL_GetTextureSize (floats).
static bool _tex_info(SDL_Texture *tex, Uint32 *format, int *access, int *w, int *h) {
    SDL_PropertiesID props = SDL_GetTextureProperties(tex);
    if(0 == props) {
        return false;
    }
    if(format) {
        *format = (Uint32)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_FORMAT_NUMBER, SDL_PIXELFORMAT_UNKNOWN);
    }
    if(access) {
        *access = (int)SDL_GetNumberProperty(props, SDL_PROP_TEXTURE_ACCESS_NUMBER, SDL_TEXTUREACCESS_STATIC);
    }
    if(w || h) {
        float fw = 0, fh = 0;
        if(!SDL_GetTextureSize(tex, &fw, &fh)) {
            return false;
        }
        if(w) *w = (int)fw;
        if(h) *h = (int)fh;
    }
    return true;
}

static PyObject * PySDL_Texture_Query(PySDL_Texture *self, PyObject *ign) {
    Uint32 format = 0;
    int access = 0, width = 0, height = 0;
    if(!_tex_info(self->texture, &format, &access, &width, &height)) {
        return _raise();
    }
    return Py_BuildValue("(Iiii)", format, access, width, height);
}

static PyObject * PySDL_Texture_Update(PySDL_Texture *self, PyObject *args, PyObject *kwds) {
    Py_buffer pixels;
    PyObject *rect_py = NULL;
    int pitch = 0;

    static char *kwlist[] = {"pixels", "rect", "pitch", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "y*|Oi", kwlist, &pixels, &rect_py, &pitch)) {
        return NULL;
    }

    SDL_Rect rect;
    SDL_Rect *rp = NULL;
    if(rect_py && rect_py != Py_None) {
        if(!PyToRect(rect_py, &rect)) {
            PyBuffer_Release(&pixels);
            return NULL;
        }
        rp = &rect;
    }

    if(pitch <= 0) {
        Uint32 format = 0;
        int width = 0;
        if(!_tex_info(self->texture, &format, NULL, &width, NULL)) {
            PyBuffer_Release(&pixels);
            return _raise();
        }
        pitch = (rp ? rect.w : width) * SDL_BYTESPERPIXEL(format);
    }

    bool ok = SDL_UpdateTexture(self->texture, rp, pixels.buf, pitch);
    PyBuffer_Release(&pixels);
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Texture_UpdateYUV(PySDL_Texture *self, PyObject *args, PyObject *kwds) {
    Py_buffer yplane, uplane, vplane;
    int ypitch, upitch, vpitch;
    PyObject *rect_py = NULL;

    static char *kwlist[] = {"yplane", "ypitch", "uplane", "upitch", "vplane", "vpitch", "rect", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "y*iy*iy*i|O", kwlist,
        &yplane, &ypitch, &uplane, &upitch, &vplane, &vpitch, &rect_py)) {
        return NULL;
    }

    SDL_Rect rect;
    SDL_Rect *rp = NULL;
    bool ok = true;
    if(rect_py && rect_py != Py_None) {
        ok = PyToRect(rect_py, &rect);
        rp = ok ? &rect : NULL;
    }

    if(ok) {
        ok = SDL_UpdateYUVTexture(self->texture, rp,
            yplane.buf, ypitch, uplane.buf, upitch, vplane.buf, vpitch);
    }

    PyBuffer_Release(&yplane);
    PyBuffer_Release(&uplane);
    PyBuffer_Release(&vplane);

    if(!ok) {
        return PyErr_Occurred() ? NULL : _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Texture_Lock(PySDL_Texture *self, PyObject *args) {
    PyObject *rect_py = NULL;
    if(!PyArg_ParseTuple(args, "|O", &rect_py)) {
        return NULL;
    }

    SDL_Rect rect;
    SDL_Rect *rp = NULL;
    if(rect_py && rect_py != Py_None) {
        if(!PyToRect(rect_py, &rect)) {
            return NULL;
        }
        rp = &rect;
    }

    void *pixels = NULL;
    int pitch = 0;
    if(!SDL_LockTexture(self->texture, rp, &pixels, &pitch)) {
        return _raise();
    }

    int height;
    if(rp) {
        height = rect.h;
    } else if(!_tex_info(self->texture, NULL, NULL, NULL, &height)) {
        SDL_UnlockTexture(self->texture);
        return _raise();
    }

    // Writable view of the locked region; only valid until Unlock().
    PyObject *view = PyMemoryView_FromMemory((char *)pixels, (Py_ssize_t)pitch * height, PyBUF_WRITE);
    if(NULL == view) {
        SDL_UnlockTexture(self->texture);
        return NULL;
    }

    return Py_BuildValue("(Ni)", view, pitch);
}

static PyObject * PySDL_Texture_Unlock(PySDL_Texture *self, PyObject *ign) {
    SDL_UnlockTexture(self->texture);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Texture_SetColorMod(PySDL_Texture *self, PyObject *args) {
    unsigned char r, g, b;
    if(!PyArg_ParseTuple(args, "bbb", &r, &g, &b)) {
        return NULL;
    }
    if(!SDL_SetTextureColorMod(self->texture, r, g, b)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Texture_GetColorMod(PySDL_Texture *self, PyObject *ign) {
    Uint8 r = 0, g = 0, b = 0;
    if(!SDL_GetTextureColorMod(self->texture, &r, &g, &b)) {
        return _raise();
    }
    return Py_BuildValue("(iii)", r, g, b);
}

static PyObject * PySDL_Texture_SetAlphaMod(PySDL_Texture *self, PyObject *arg) {
    long a = PyLong_AsLong(arg);
    if(-1 == a && PyErr_Occurred()) {
        return NULL;
    }
    if(!SDL_SetTextureAlphaMod(self->texture, (Uint8)a)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Texture_GetAlphaMod(PySDL_Texture *self, PyObject *ign) {
    Uint8 a = 0;
    if(!SDL_GetTextureAlphaMod(self->texture, &a)) {
        return _raise();
    }
    return PyLong_FromLong(a);
}

static PyObject * PySDL_Texture_SetBlendMode(PySDL_Texture *self, PyObject *arg) {
    long mode = PyLong_AsLong(arg);
    if(-1 == mode && PyErr_Occurred()) {
        return NULL;
    }
    if(!SDL_SetTextureBlendMode(self->texture, (SDL_BlendMode)mode)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Texture_GetBlendMode(PySDL_Texture *self, PyObject *ign) {
    SDL_BlendMode mode = SDL_BLENDMODE_NONE;
    if(!SDL_GetTextureBlendMode(self->texture, &mode)) {
        return _raise();
    }
    return PyLong_FromLong(mode);
}

static PyObject * PySDL_Texture_SetScaleMode(PySDL_Texture *self, PyObject *arg) {
    long mode = PyLong_AsLong(arg);
    if(-1 == mode && PyErr_Occurred()) {
        return NULL;
    }
    if(!SDL_SetTextureScaleMode(self->texture, (SDL_ScaleMode)mode)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Texture_GetScaleMode(PySDL_Texture *self, PyObject *ign) {
    SDL_ScaleMode mode = SDL_SCALEMODE_NEAREST;
    if(!SDL_GetTextureScaleMode(self->texture, &mode)) {
        return _raise();
    }
    return PyLong_FromLong(mode);
}
