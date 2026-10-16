#include "pysdl.h"

static int        PySDL_Texture_Type_init    (PySDL_Texture*, PyObject*, PyObject*);
static void       PySDL_Texture_Type_dealloc (PySDL_Texture*);

static PyObject * PySDL_Texture_Query        (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_Update       (PySDL_Texture*, PyObject*, PyObject*);
static PyObject * PySDL_Texture_UpdateYUV    (PySDL_Texture*, PyObject*, PyObject*);
static PyObject * PySDL_Texture_UpdateNV     (PySDL_Texture*, PyObject*, PyObject*);
static PyObject * PySDL_Texture_Lock         (PySDL_Texture*, PyObject*);
static PyObject * PySDL_Texture_LockToSurface (PySDL_Texture*, PyObject*);
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
    { "UpdateNV",     (PyCFunction)PySDL_Texture_UpdateNV,     METH_VARARGS | METH_KEYWORDS },
    { "Lock",         (PyCFunction)PySDL_Texture_Lock,         METH_VARARGS },
    { "LockToSurface", (PyCFunction)PySDL_Texture_LockToSurface, METH_VARARGS },
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
    self->locked = NULL;
    self->renderer = NULL;

    // No renderer: internal allocation; the caller fills in ->texture and
    // registers it with PySDL_TextureTrack.
    if(renderer && renderer != Py_None) {
        if(!PyObject_TypeCheck(renderer, &PySDL_Renderer_Type)) {
            PyErr_SetString(PyExc_TypeError, "renderer must be an SDL3.Renderer");
            return -1;
        }
        SDL_Renderer *owner = ((PySDL_Renderer *)renderer)->renderer;
        self->texture = SDL_CreateTexture(owner, (SDL_PixelFormat)format, (SDL_TextureAccess)access, w, h);
        if(NULL == self->texture) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
        if(0 > PySDL_TextureTrack(self, owner)) {
            return -1;  // dealloc destroys the texture
        }
    }

    return 0;
}

// SDL frees the LockToSurface surface on unlock and with the texture: point
// the Python Surface at an empty one of its own first, so it never dangles.
static void _detach_locked(PySDL_Texture *self) {
    if(NULL != self->locked) {
        PySDL_SurfaceDetach((PySDL_Surface *)self->locked);
        Py_CLEAR(self->locked);
    }
}

// The texture's renderer is being destroyed and takes the texture with it:
// drop the pointer (methods then raise) and empty any locked Surface.
void PySDL_TextureInvalidate(PySDL_Texture *self) {
    _detach_locked(self);
    self->texture = NULL;
    self->renderer = NULL;
}

static void PySDL_Texture_Type_dealloc(PySDL_Texture *self) {
    PySDL_TextureForget(self);
    _detach_locked(self);
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

// An optional rect argument (None = the whole texture) and the area SDL
// actually touches, the rect clipped to the texture: 1 with *rp (NULL for the
// whole texture) and *area set, 0 if the rect misses the texture, or -1 with
// an exception set. SDL_UpdateYUVTexture / SDL_UpdateNVTexture don't check
// that the clip is empty and copy rows of negative width, so callers stop at 0.
static int _clip_rect(PySDL_Texture *self, PyObject *rect_py, SDL_Rect *rect, SDL_Rect **rp, SDL_Rect *area) {
    int w, h;
    if(!_tex_info(self->texture, NULL, NULL, &w, &h)) {
        _raise();
        return -1;
    }
    SDL_Rect full = { 0, 0, w, h };
    *area = full;
    *rp = NULL;
    if(rect_py && rect_py != Py_None) {
        if(!PyToRect(rect_py, rect)) {
            return -1;
        }
        *rp = rect;
        if(!SDL_GetRectIntersection(&full, rect, area)) {
            return 0;
        }
    }
    return 1;
}

static PyObject * PySDL_Texture_Query(PySDL_Texture *self, PyObject *ign) {
    Uint32 format = 0;
    int access = 0, width = 0, height = 0;
    if(!_tex_info(self->texture, &format, &access, &width, &height)) {
        return _raise();
    }
    return Py_BuildValue("(Iiii)", format, access, width, height);
}

// Update(pixels, rect=None, pitch=0): pitch 0 = rows of the rect (or texture)
// width, tightly packed. SDL reads the rect clipped to the texture (for a YUV
// texture, the chroma planes after the luma rows); the buffer must cover that.
static PyObject * PySDL_Texture_Update(PySDL_Texture *self, PyObject *args, PyObject *kwds) {
    Py_buffer pixels;
    PyObject *rect_py = NULL;
    int pitch = 0;

    static char *kwlist[] = {"pixels", "rect", "pitch", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "y*|Oi", kwlist, &pixels, &rect_py, &pitch)) {
        return NULL;
    }

    PyObject *result = NULL;
    SDL_Rect rect, area;
    SDL_Rect *rp;
    Uint32 format = 0;
    int hit = _clip_rect(self, rect_py, &rect, &rp, &area);
    if(0 >= hit) {
        result = hit ? NULL : Py_NewRef(Py_None);  // missing the texture is a no-op, as in SDL
        goto done;
    }
    if(!_tex_info(self->texture, &format, NULL, NULL, NULL)) {
        result = _raise();
        goto done;
    }
    if(pitch <= 0) {
        pitch = 0;  // tight rows of the caller's rect
        if(0 > PySDL_PixelsSize((SDL_PixelFormat)format, rp ? rect.w : area.w, 1, &pitch)) {
            goto done;
        }
    }
    Py_ssize_t need = PySDL_PixelsSize((SDL_PixelFormat)format, area.w, area.h, &pitch);
    if(0 > need) {
        goto done;
    }
    if(pixels.len < need) {
        PyErr_Format(PyExc_ValueError, "pixels holds %zd bytes, the update needs %zd", pixels.len, need);
        goto done;
    }

    if(!SDL_UpdateTexture(self->texture, rp, pixels.buf, pitch)) {
        result = _raise();
        goto done;
    }
    result = Py_NewRef(Py_None);

done:
    PyBuffer_Release(&pixels);
    return result;
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

    PyObject *result = NULL;
    SDL_Rect rect, area;
    SDL_Rect *rp;
    int hit = _clip_rect(self, rect_py, &rect, &rp, &area);
    if(0 >= hit) {
        result = hit ? NULL : Py_NewRef(Py_None);
        goto done;
    }

    // SDL copies the update rect clipped to the texture: h rows of w bytes from
    // Y and (h+1)/2 rows of (w+1)/2 bytes from U and V. Check the pitches and
    // buffers cover that rather than let it read past the end of a short buffer.
    int cw = (area.w + 1) / 2, ch = (area.h + 1) / 2;
    if(ypitch < area.w || upitch < cw || vpitch < cw
        || yplane.len < (Py_ssize_t)ypitch * area.h
        || uplane.len < (Py_ssize_t)upitch * ch
        || vplane.len < (Py_ssize_t)vpitch * ch) {
        PyErr_SetString(PyExc_ValueError,
            "plane buffers or pitches are too small for the update area");
        goto done;
    }

    if(!SDL_UpdateYUVTexture(self->texture, rp,
            yplane.buf, ypitch, uplane.buf, upitch, vplane.buf, vpitch)) {
        result = _raise();
        goto done;
    }
    result = Py_NewRef(Py_None);

done:
    PyBuffer_Release(&yplane);
    PyBuffer_Release(&uplane);
    PyBuffer_Release(&vplane);
    return result;
}

// UpdateNV(yplane, ypitch, uvplane, uvpitch, rect=None) for NV12 / NV21
// textures, checked like UpdateYUV: SDL copies h rows of w bytes from Y and
// (h+1)/2 rows of w (rounded up to even) bytes of interleaved UV.
static PyObject * PySDL_Texture_UpdateNV(PySDL_Texture *self, PyObject *args, PyObject *kwds) {
    Py_buffer yplane, uvplane;
    int ypitch, uvpitch;
    PyObject *rect_py = NULL;

    static char *kwlist[] = {"yplane", "ypitch", "uvplane", "uvpitch", "rect", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "y*iy*i|O", kwlist,
        &yplane, &ypitch, &uvplane, &uvpitch, &rect_py)) {
        return NULL;
    }

    PyObject *result = NULL;
    SDL_Rect rect, area;
    SDL_Rect *rp;
    int hit = _clip_rect(self, rect_py, &rect, &rp, &area);
    if(0 >= hit) {
        result = hit ? NULL : Py_NewRef(Py_None);
        goto done;
    }

    int cw = 2 * ((area.w + 1) / 2), ch = (area.h + 1) / 2;
    if(ypitch < area.w || uvpitch < cw
        || yplane.len < (Py_ssize_t)ypitch * area.h
        || uvplane.len < (Py_ssize_t)uvpitch * ch) {
        PyErr_SetString(PyExc_ValueError,
            "plane buffers or pitches are too small for the update area");
        goto done;
    }

    if(!SDL_UpdateNVTexture(self->texture, rp, yplane.buf, ypitch, uvplane.buf, uvpitch)) {
        result = _raise();
        goto done;
    }
    result = Py_NewRef(Py_None);

done:
    PyBuffer_Release(&yplane);
    PyBuffer_Release(&uvplane);
    return result;
}

// Lock(rect=None) -> (writable memoryview, pitch) over the locked region of a
// TEXTUREACCESS_STREAMING texture, valid until Unlock(). SDL doesn't clip the
// rect, so it must lie within the texture, and the view ends with the rect's
// last row: rows are `pitch` apart, but the last one is only w pixels long.
static PyObject * PySDL_Texture_Lock(PySDL_Texture *self, PyObject *args) {
    PyObject *rect_py = NULL;
    if(!PyArg_ParseTuple(args, "|O", &rect_py)) {
        return NULL;
    }

    SDL_Rect rect, area;
    SDL_Rect *rp;
    int hit = _clip_rect(self, rect_py, &rect, &rp, &area);
    if(0 > hit) {
        return NULL;
    }
    if(rp && (!hit || area.x != rect.x || area.y != rect.y || area.w != rect.w || area.h != rect.h)) {
        PyErr_SetString(PyExc_ValueError, "rect must lie within the texture");
        return NULL;
    }
    Uint32 format = 0;
    if(!_tex_info(self->texture, &format, NULL, NULL, NULL)) {
        return _raise();
    }

    void *pixels = NULL;
    int pitch = 0;
    if(!SDL_LockTexture(self->texture, rp, &pixels, &pitch)) {
        return _raise();
    }

    // Writable view of the locked region; only valid until Unlock().
    Py_ssize_t size = (Py_ssize_t)pitch * (area.h - 1) + (Py_ssize_t)area.w * SDL_BYTESPERPIXEL(format);
    PyObject *view = PyMemoryView_FromMemory((char *)pixels, size, PyBUF_WRITE);
    if(NULL == view) {
        SDL_UnlockTexture(self->texture);
        return NULL;
    }

    return Py_BuildValue("(Ni)", view, pitch);
}

// LockToSurface(rect=None) -> Surface over the locked (write-only) region of a
// TEXTUREACCESS_STREAMING texture, the rect clipped to the texture as SDL
// does. Draw into it, then Unlock() to upload; SDL frees the surface then, so
// the Surface object is emptied (0x0) rather than left dangling.
static PyObject * PySDL_Texture_LockToSurface(PySDL_Texture *self, PyObject *args) {
    PyObject *rect_py = NULL;
    if(!PyArg_ParseTuple(args, "|O", &rect_py)) {
        return NULL;
    }
    if(NULL != self->locked) {
        PyErr_SetString(pysdl_Error, "Texture is already locked to a surface");
        return NULL;
    }

    SDL_Rect rect, area;
    SDL_Rect *rp;
    int hit = _clip_rect(self, rect_py, &rect, &rp, &area);
    if(0 > hit) {
        return NULL;
    }
    if(!hit) {
        PyErr_SetString(PyExc_ValueError, "rect does not overlap the texture");
        return NULL;
    }

    SDL_Surface *locked = NULL;
    if(!SDL_LockTextureToSurface(self->texture, rp ? &area : NULL, &locked)) {
        return _raise();
    }
    PySDL_Surface *wrapper = (PySDL_Surface *)PySDL_New(&PySDL_Surface_Type);
    if(NULL == wrapper) {
        SDL_UnlockTexture(self->texture);
        return NULL;
    }
    wrapper->surface = locked;  // the texture's until Unlock
    wrapper->shouldFree = 0;
    self->locked = Py_NewRef((PyObject *)wrapper);
    return (PyObject *)wrapper;
}

static PyObject * PySDL_Texture_Unlock(PySDL_Texture *self, PyObject *ign) {
    _detach_locked(self);  // SDL frees the LockToSurface surface
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
