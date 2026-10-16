#include "pysdl.h"

static int        PySDL_Renderer_Type_init    (PySDL_Renderer*, PyObject*, PyObject*);
static void       PySDL_Renderer_Type_dealloc (PySDL_Renderer*);

static PyObject * PySDL_Renderer_CreateTextureFromSurface (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_LoadTexture              (PySDL_Renderer*, PyObject*, PyObject*);

static PyObject * PySDL_Renderer_Clear             (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_Present           (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_Flush             (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_SetVSync          (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_Copy              (PySDL_Renderer*, PyObject*, PyObject*);
static PyObject * PySDL_Renderer_CopyEx            (PySDL_Renderer*, PyObject*, PyObject*);

static PyObject * PySDL_Renderer_DrawPoint         (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_DrawPoints        (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_DrawLine          (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_DrawLines         (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_DrawRect          (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_DrawRects         (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_FillRect          (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_FillRects         (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_RenderGeometry    (PySDL_Renderer*, PyObject*, PyObject*);

static PyObject * PySDL_Renderer_SetRenderDrawColor     (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_GetRenderDrawColor     (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_SetRenderDrawBlendMode (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_GetRenderDrawBlendMode (PySDL_Renderer*, PyObject*);

static PyObject * PySDL_Renderer_SetViewport       (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_GetViewport       (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_SetClipRect       (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_GetClipRect       (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_IsClipEnabled     (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_SetScale          (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_GetScale          (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_SetLogicalSize    (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_GetLogicalSize    (PySDL_Renderer*, PyObject*);

static PyObject * PySDL_Renderer_SetRenderTarget      (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_GetRenderTarget      (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_ReadPixels           (PySDL_Renderer*, PyObject*, PyObject*);

static PyObject * PySDL_Renderer_GetRendererInfo       (PySDL_Renderer*, PyObject*);
static PyObject * PySDL_Renderer_GetRendererOutputSize (PySDL_Renderer*, PyObject*);

static PyMethodDef PySDL_Renderer_methods[] = {
    { "CreateTextureFromSurface", (PyCFunction)PySDL_Renderer_CreateTextureFromSurface, METH_O       },
    { "LoadTexture",              (PyCFunction)PySDL_Renderer_LoadTexture,              METH_VARARGS | METH_KEYWORDS },

    { "Clear",                    (PyCFunction)PySDL_Renderer_Clear,                    METH_NOARGS  },
    { "Present",                  (PyCFunction)PySDL_Renderer_Present,                  METH_NOARGS  },
    { "RenderFlush",              (PyCFunction)PySDL_Renderer_Flush,                    METH_NOARGS  },
    { "SetVSync",                 (PyCFunction)PySDL_Renderer_SetVSync,                 METH_O       },
    // In SDL3 SDL_RenderTexture takes float rects; these four names are kept as
    // aliases for the two underlying calls.
    { "Copy",                     (PyCFunction)PySDL_Renderer_Copy,                     METH_VARARGS | METH_KEYWORDS },
    { "CopyF",                    (PyCFunction)PySDL_Renderer_Copy,                     METH_VARARGS | METH_KEYWORDS },
    { "CopyEx",                   (PyCFunction)PySDL_Renderer_CopyEx,                   METH_VARARGS | METH_KEYWORDS },
    { "CopyExF",                  (PyCFunction)PySDL_Renderer_CopyEx,                   METH_VARARGS | METH_KEYWORDS },
    { "RenderTexture",            (PyCFunction)PySDL_Renderer_Copy,                     METH_VARARGS | METH_KEYWORDS },
    { "RenderTextureRotated",     (PyCFunction)PySDL_Renderer_CopyEx,                   METH_VARARGS | METH_KEYWORDS },

    { "DrawPoint",                (PyCFunction)PySDL_Renderer_DrawPoint,                METH_VARARGS },
    { "DrawPoints",               (PyCFunction)PySDL_Renderer_DrawPoints,               METH_O       },
    { "DrawLine",                 (PyCFunction)PySDL_Renderer_DrawLine,                 METH_VARARGS },
    { "DrawLines",                (PyCFunction)PySDL_Renderer_DrawLines,                METH_O       },
    { "DrawRect",                 (PyCFunction)PySDL_Renderer_DrawRect,                 METH_O       },
    { "DrawRects",                (PyCFunction)PySDL_Renderer_DrawRects,                METH_O       },
    { "FillRect",                 (PyCFunction)PySDL_Renderer_FillRect,                 METH_O       },
    { "FillRects",                (PyCFunction)PySDL_Renderer_FillRects,                METH_O       },
    { "RenderGeometry",           (PyCFunction)PySDL_Renderer_RenderGeometry,           METH_VARARGS | METH_KEYWORDS },

    { "SetRenderDrawColor",       (PyCFunction)PySDL_Renderer_SetRenderDrawColor,       METH_VARARGS },
    { "GetRenderDrawColor",       (PyCFunction)PySDL_Renderer_GetRenderDrawColor,       METH_NOARGS  },
    { "SetRenderDrawBlendMode",   (PyCFunction)PySDL_Renderer_SetRenderDrawBlendMode,   METH_O       },
    { "GetRenderDrawBlendMode",   (PyCFunction)PySDL_Renderer_GetRenderDrawBlendMode,   METH_NOARGS  },

    { "RenderSetViewport",        (PyCFunction)PySDL_Renderer_SetViewport,              METH_O       },
    { "RenderGetViewport",        (PyCFunction)PySDL_Renderer_GetViewport,              METH_NOARGS  },
    { "RenderSetClipRect",        (PyCFunction)PySDL_Renderer_SetClipRect,              METH_O       },
    { "RenderGetClipRect",        (PyCFunction)PySDL_Renderer_GetClipRect,              METH_NOARGS  },
    { "RenderIsClipEnabled",      (PyCFunction)PySDL_Renderer_IsClipEnabled,            METH_NOARGS  },
    { "RenderSetScale",           (PyCFunction)PySDL_Renderer_SetScale,                 METH_VARARGS },
    { "RenderGetScale",           (PyCFunction)PySDL_Renderer_GetScale,                 METH_NOARGS  },
    { "RenderSetLogicalSize",     (PyCFunction)PySDL_Renderer_SetLogicalSize,           METH_VARARGS },
    { "RenderGetLogicalSize",     (PyCFunction)PySDL_Renderer_GetLogicalSize,           METH_NOARGS  },

    { "SetRenderTarget",          (PyCFunction)PySDL_Renderer_SetRenderTarget,          METH_O       },
    { "GetRenderTarget",          (PyCFunction)PySDL_Renderer_GetRenderTarget,          METH_NOARGS  },
    { "RenderReadPixels",         (PyCFunction)PySDL_Renderer_ReadPixels,               METH_VARARGS | METH_KEYWORDS },

    { "GetRendererInfo",          (PyCFunction)PySDL_Renderer_GetRendererInfo,          METH_NOARGS  },
    { "GetRendererOutputSize",    (PyCFunction)PySDL_Renderer_GetRendererOutputSize,    METH_NOARGS  },

    { NULL }
};

PyTypeObject PySDL_Renderer_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Renderer",
    .tp_basicsize = sizeof(PySDL_Renderer),
    .tp_dealloc   = (destructor)PySDL_Renderer_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Renderer(window, name=None)",
    .tp_methods   = PySDL_Renderer_methods,
    .tp_init      = (initproc)PySDL_Renderer_Type_init,
    .tp_new       = PyType_GenericNew,
};

static int PySDL_Renderer_Type_init(PySDL_Renderer *self, PyObject *args, PyObject *kwds) {
    PyObject *window = NULL;
    const char *name = NULL;

    static char *kwlist[] = {"window", "name", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|Oz", kwlist, &window, &name)) {
        return -1;
    }

    self->renderer = NULL;
    self->target = NULL;

    // No window: internal allocation (Window.CreateRenderer, CreateSoftwareRenderer)
    // fills in ->renderer afterwards.
    if(window && window != Py_None) {
        if(!PyObject_TypeCheck(window, &PySDL_Window_Type)) {
            PyErr_SetString(PyExc_TypeError, "window must be an SDL3.Window");
            return -1;
        }
        self->renderer = SDL_CreateRenderer(((PySDL_Window *)window)->window, name);
        if(NULL == self->renderer) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }

    return 0;
}

static void PySDL_Renderer_Type_dealloc(PySDL_Renderer *self) {
    Py_XDECREF(self->target);
    if(NULL != self->renderer) {
        SDL_DestroyRenderer(self->renderer);
        self->renderer = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

//=========================================================
// helpers
//=========================================================

static PyObject * _raise(void) {
    PyErr_SetString(pysdl_Error, SDL_GetError());
    return NULL;
}

// tuple/list of (x, y) -> malloc'd SDL_FPoint[]; caller PyMem_Free()s.
static SDL_FPoint * _fpoints(PyObject *seq, int *count) {
    PyObject *fast = PySequence_Fast(seq, "expected a list of (x, y) points");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    SDL_FPoint *pts = PyMem_New(SDL_FPoint, n > 0 ? n : 1);
    if(NULL == pts) {
        Py_DECREF(fast);
        PyErr_NoMemory();
        return NULL;
    }
    for(Py_ssize_t idx = 0; idx < n; ++idx) {
        if(!PyToFPoint(PySequence_Fast_GET_ITEM(fast, idx), &pts[idx])) {
            PyMem_Free(pts);
            Py_DECREF(fast);
            return NULL;
        }
    }
    Py_DECREF(fast);
    *count = (int)n;
    return pts;
}

// tuple/list of (x, y, w, h) -> malloc'd SDL_FRect[]; caller PyMem_Free()s.
static SDL_FRect * _frects(PyObject *seq, int *count) {
    PyObject *fast = PySequence_Fast(seq, "expected a list of (x, y, w, h) rects");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    SDL_FRect *rects = PyMem_New(SDL_FRect, n > 0 ? n : 1);
    if(NULL == rects) {
        Py_DECREF(fast);
        PyErr_NoMemory();
        return NULL;
    }
    for(Py_ssize_t idx = 0; idx < n; ++idx) {
        if(!PyToFRect(PySequence_Fast_GET_ITEM(fast, idx), &rects[idx])) {
            PyMem_Free(rects);
            Py_DECREF(fast);
            return NULL;
        }
    }
    Py_DECREF(fast);
    *count = (int)n;
    return rects;
}

//=========================================================
// textures
//=========================================================

static PyObject * PySDL_Renderer_CreateTextureFromSurface(PySDL_Renderer *self, PyObject *arg) {
    if(!PyObject_TypeCheck(arg, &PySDL_Surface_Type)) {
        PyErr_SetString(PyExc_TypeError, "expected an SDL3.Surface");
        return NULL;
    }

    PySDL_Texture *pysdl_Texture = (PySDL_Texture *)PySDL_New(&PySDL_Texture_Type);
    if(NULL == pysdl_Texture) {
        return NULL;
    }

    pysdl_Texture->texture = SDL_CreateTextureFromSurface(self->renderer, ((PySDL_Surface *)arg)->surface);
    if(NULL == pysdl_Texture->texture) {
        Py_DECREF(pysdl_Texture);
        return _raise();
    }

    return (PyObject *)pysdl_Texture;
}

// LoadTexture(src, type=None): `src` is a path (str / os.PathLike) or the
// file's bytes; `type` names a magic-less format ("TGA"), as for LoadImage.
static PyObject * PySDL_Renderer_LoadTexture(PySDL_Renderer *self, PyObject *args, PyObject *kwds) {
    PyObject *src;
    const char *type = NULL;
    static char *kwlist[] = {"src", "type", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|z", kwlist, &src, &type)) {
        return NULL;
    }

    SDL_Texture *texture = PySDL_IMG_LoadTexture(self->renderer, src, type);
    if(NULL == texture) {
        return NULL;
    }
    PySDL_Texture *wrapper = (PySDL_Texture *)PySDL_New(&PySDL_Texture_Type);
    if(NULL == wrapper) {
        SDL_DestroyTexture(texture);
        return NULL;
    }
    wrapper->texture = texture;
    return (PyObject *)wrapper;
}

//=========================================================
// frame
//=========================================================

static PyObject * PySDL_Renderer_Clear(PySDL_Renderer *self, PyObject *ign) {
    if(!SDL_RenderClear(self->renderer)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_Present(PySDL_Renderer *self, PyObject *ign) {
    SDL_RenderPresent(self->renderer);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_Flush(PySDL_Renderer *self, PyObject *ign) {
    if(!SDL_FlushRenderer(self->renderer)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_SetVSync(PySDL_Renderer *self, PyObject *arg) {
    long vsync = PyLong_AsLong(arg);
    if(-1 == vsync && PyErr_Occurred()) {
        return NULL;
    }
    if(!SDL_SetRenderVSync(self->renderer, (int)vsync)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

//=========================================================
// copy  (SDL_RenderTexture / SDL_RenderTextureRotated - float rects)
//=========================================================

static PyObject * PySDL_Renderer_Copy(PySDL_Renderer *self, PyObject *args, PyObject *kwds) {
    PySDL_Texture *texture = NULL;
    PyObject *src_py = NULL;
    PyObject *dst_py = NULL;
    SDL_FRect src_rect, dst_rect;
    SDL_FRect *src = NULL, *dst = NULL;

    static char *kwlist[] = {"texture", "src", "dst", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O!|OO", kwlist,
        &PySDL_Texture_Type, &texture, &src_py, &dst_py)) {
        return NULL;
    }

    if(src_py && src_py != Py_None) {
        if(!PyToFRect(src_py, &src_rect)) {
            return NULL;
        }
        src = &src_rect;
    }
    if(dst_py && dst_py != Py_None) {
        if(!PyToFRect(dst_py, &dst_rect)) {
            return NULL;
        }
        dst = &dst_rect;
    }

    if(!SDL_RenderTexture(self->renderer, texture->texture, src, dst)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_CopyEx(PySDL_Renderer *self, PyObject *args, PyObject *kwds) {
    PySDL_Texture *texture = NULL;
    PyObject *src_py = NULL;
    PyObject *dst_py = NULL;
    PyObject *center_py = NULL;
    SDL_FRect src_rect, dst_rect;
    SDL_FRect *src = NULL, *dst = NULL;
    SDL_FPoint center_point;
    SDL_FPoint *center = NULL;
    double angle = 0.0;
    unsigned int flip = SDL_FLIP_NONE;

    static char *kwlist[] = {"texture", "src", "dst", "angle", "center", "flip", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O!|OOdOI", kwlist,
        &PySDL_Texture_Type, &texture, &src_py, &dst_py, &angle, &center_py, &flip)) {
        return NULL;
    }

    if(src_py && src_py != Py_None) {
        if(!PyToFRect(src_py, &src_rect)) {
            return NULL;
        }
        src = &src_rect;
    }
    if(dst_py && dst_py != Py_None) {
        if(!PyToFRect(dst_py, &dst_rect)) {
            return NULL;
        }
        dst = &dst_rect;
    }
    if(center_py && center_py != Py_None) {
        if(!PyToFPoint(center_py, &center_point)) {
            return NULL;
        }
        center = &center_point;
    }

    if(!SDL_RenderTextureRotated(self->renderer, texture->texture, src, dst, angle, center, (SDL_FlipMode)flip)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

//=========================================================
// primitives (float coordinates; ints are accepted too)
//=========================================================

static PyObject * PySDL_Renderer_DrawPoint(PySDL_Renderer *self, PyObject *args) {
    float x, y;
    if(!PyArg_ParseTuple(args, "ff", &x, &y)) {
        return NULL;
    }
    if(!SDL_RenderPoint(self->renderer, x, y)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_DrawPoints(PySDL_Renderer *self, PyObject *arg) {
    int count = 0;
    SDL_FPoint *pts = _fpoints(arg, &count);
    if(NULL == pts) {
        return NULL;
    }
    bool ok = SDL_RenderPoints(self->renderer, pts, count);
    PyMem_Free(pts);
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_DrawLine(PySDL_Renderer *self, PyObject *args) {
    float x1, y1, x2, y2;
    if(!PyArg_ParseTuple(args, "ffff", &x1, &y1, &x2, &y2)) {
        return NULL;
    }
    if(!SDL_RenderLine(self->renderer, x1, y1, x2, y2)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_DrawLines(PySDL_Renderer *self, PyObject *arg) {
    int count = 0;
    SDL_FPoint *pts = _fpoints(arg, &count);
    if(NULL == pts) {
        return NULL;
    }
    bool ok = SDL_RenderLines(self->renderer, pts, count);
    PyMem_Free(pts);
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_DrawRect(PySDL_Renderer *self, PyObject *arg) {
    SDL_FRect rect;
    SDL_FRect *rp = NULL;
    if(arg != Py_None) {
        if(!PyToFRect(arg, &rect)) {
            return NULL;
        }
        rp = &rect;
    }
    if(!SDL_RenderRect(self->renderer, rp)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_DrawRects(PySDL_Renderer *self, PyObject *arg) {
    int count = 0;
    SDL_FRect *rects = _frects(arg, &count);
    if(NULL == rects) {
        return NULL;
    }
    bool ok = SDL_RenderRects(self->renderer, rects, count);
    PyMem_Free(rects);
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_FillRect(PySDL_Renderer *self, PyObject *arg) {
    SDL_FRect rect;
    SDL_FRect *rp = NULL;
    if(arg != Py_None) {
        if(!PyToFRect(arg, &rect)) {
            return NULL;
        }
        rp = &rect;
    }
    if(!SDL_RenderFillRect(self->renderer, rp)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_FillRects(PySDL_Renderer *self, PyObject *arg) {
    int count = 0;
    SDL_FRect *rects = _frects(arg, &count);
    if(NULL == rects) {
        return NULL;
    }
    bool ok = SDL_RenderFillRects(self->renderer, rects, count);
    PyMem_Free(rects);
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_RenderGeometry(PySDL_Renderer *self, PyObject *args, PyObject *kwds) {
    PyObject *texture_py = NULL;
    PyObject *vertices_py = NULL;
    PyObject *indices_py = NULL;

    static char *kwlist[] = {"texture", "vertices", "indices", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "OO|O", kwlist, &texture_py, &vertices_py, &indices_py)) {
        return NULL;
    }

    SDL_Texture *texture = NULL;
    if(texture_py != Py_None) {
        if(!PyObject_TypeCheck(texture_py, &PySDL_Texture_Type)) {
            PyErr_SetString(PyExc_TypeError, "texture must be an SDL3.Texture or None");
            return NULL;
        }
        texture = ((PySDL_Texture *)texture_py)->texture;
    }

    PyObject *vfast = PySequence_Fast(vertices_py, "vertices must be a list");
    if(NULL == vfast) {
        return NULL;
    }
    Py_ssize_t nverts = PySequence_Fast_GET_SIZE(vfast);
    SDL_Vertex *verts = PyMem_New(SDL_Vertex, nverts > 0 ? nverts : 1);
    if(NULL == verts) {
        Py_DECREF(vfast);
        return PyErr_NoMemory();
    }

    for(Py_ssize_t idx = 0; idx < nverts; ++idx) {
        PyObject *fast = PySequence_Fast(PySequence_Fast_GET_ITEM(vfast, idx),
            "each vertex must be (position, color[, tex_coord])");
        if(NULL == fast) {
            goto vertex_error;
        }
        Py_ssize_t parts = PySequence_Fast_GET_SIZE(fast);
        SDL_FPoint uv = {0.0f, 0.0f};
        SDL_Color rgba;
        if((parts != 2 && parts != 3)
            || !PyToFPoint(PySequence_Fast_GET_ITEM(fast, 0), &verts[idx].position)
            || !PyToColor(PySequence_Fast_GET_ITEM(fast, 1), &rgba)
            || (parts == 3 && !PyToFPoint(PySequence_Fast_GET_ITEM(fast, 2), &uv))) {
            if(parts != 2 && parts != 3) {
                PyErr_SetString(PyExc_ValueError, "each vertex must be (position, color[, tex_coord])");
            }
            Py_DECREF(fast);
            goto vertex_error;
        }
        // SDL3 vertex colours are 0..1 floats.
        verts[idx].color = (SDL_FColor){rgba.r / 255.0f, rgba.g / 255.0f, rgba.b / 255.0f, rgba.a / 255.0f};
        verts[idx].tex_coord = uv;
        Py_DECREF(fast);
    }
    Py_DECREF(vfast);
    vfast = NULL;

    int *indices = NULL;
    int nindices = 0;
    if(indices_py && indices_py != Py_None) {
        PyObject *ifast = PySequence_Fast(indices_py, "indices must be a list of ints");
        if(NULL == ifast) {
            PyMem_Free(verts);
            return NULL;
        }
        nindices = (int)PySequence_Fast_GET_SIZE(ifast);
        indices = PyMem_New(int, nindices > 0 ? nindices : 1);
        if(NULL == indices) {
            Py_DECREF(ifast);
            PyMem_Free(verts);
            return PyErr_NoMemory();
        }
        for(int i = 0; i < nindices; ++i) {
            indices[i] = (int)PyLong_AsLong(PySequence_Fast_GET_ITEM(ifast, i));
            if(-1 == indices[i] && PyErr_Occurred()) {
                Py_DECREF(ifast);
                PyMem_Free(indices);
                PyMem_Free(verts);
                return NULL;
            }
        }
        Py_DECREF(ifast);
    }

    bool ok = SDL_RenderGeometry(self->renderer, texture, verts, (int)nverts, indices, nindices);
    PyMem_Free(verts);
    PyMem_Free(indices);
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;

vertex_error:
    PyMem_Free(verts);
    Py_XDECREF(vfast);
    return NULL;
}

//=========================================================
// draw state
//=========================================================

static PyObject * PySDL_Renderer_SetRenderDrawColor(PySDL_Renderer *self, PyObject *args) {
    unsigned char r = 0, g = 0, b = 0, a = 255;
    if(!PyArg_ParseTuple(args, "bbb|b", &r, &g, &b, &a)) {
        return NULL;
    }
    if(!SDL_SetRenderDrawColor(self->renderer, r, g, b, a)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_GetRenderDrawColor(PySDL_Renderer *self, PyObject *ign) {
    Uint8 r = 0, g = 0, b = 0, a = 0;
    if(!SDL_GetRenderDrawColor(self->renderer, &r, &g, &b, &a)) {
        return _raise();
    }
    return Py_BuildValue("(iiii)", r, g, b, a);
}

static PyObject * PySDL_Renderer_SetRenderDrawBlendMode(PySDL_Renderer *self, PyObject *arg) {
    long mode = PyLong_AsLong(arg);
    if(-1 == mode && PyErr_Occurred()) {
        return NULL;
    }
    if(!SDL_SetRenderDrawBlendMode(self->renderer, (SDL_BlendMode)mode)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_GetRenderDrawBlendMode(PySDL_Renderer *self, PyObject *ign) {
    SDL_BlendMode mode = SDL_BLENDMODE_NONE;
    if(!SDL_GetRenderDrawBlendMode(self->renderer, &mode)) {
        return _raise();
    }
    return PyLong_FromLong(mode);
}

static PyObject * PySDL_Renderer_SetViewport(PySDL_Renderer *self, PyObject *arg) {
    SDL_Rect rect;
    SDL_Rect *rp = NULL;
    if(arg != Py_None) {
        if(!PyToRect(arg, &rect)) {
            return NULL;
        }
        rp = &rect;
    }
    if(!SDL_SetRenderViewport(self->renderer, rp)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_GetViewport(PySDL_Renderer *self, PyObject *ign) {
    SDL_Rect rect;
    SDL_GetRenderViewport(self->renderer, &rect);
    return RectToPy(&rect);
}

static PyObject * PySDL_Renderer_SetClipRect(PySDL_Renderer *self, PyObject *arg) {
    SDL_Rect rect;
    SDL_Rect *rp = NULL;
    if(arg != Py_None) {
        if(!PyToRect(arg, &rect)) {
            return NULL;
        }
        rp = &rect;
    }
    if(!SDL_SetRenderClipRect(self->renderer, rp)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_GetClipRect(PySDL_Renderer *self, PyObject *ign) {
    SDL_Rect rect;
    SDL_GetRenderClipRect(self->renderer, &rect);
    return RectToPy(&rect);
}

static PyObject * PySDL_Renderer_IsClipEnabled(PySDL_Renderer *self, PyObject *ign) {
    return PyBool_FromLong(SDL_RenderClipEnabled(self->renderer));
}

static PyObject * PySDL_Renderer_SetScale(PySDL_Renderer *self, PyObject *args) {
    float sx, sy;
    if(!PyArg_ParseTuple(args, "ff", &sx, &sy)) {
        return NULL;
    }
    if(!SDL_SetRenderScale(self->renderer, sx, sy)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_GetScale(PySDL_Renderer *self, PyObject *ign) {
    float sx = 1.0f, sy = 1.0f;
    SDL_GetRenderScale(self->renderer, &sx, &sy);
    return Py_BuildValue("(ff)", sx, sy);
}

static PyObject * PySDL_Renderer_SetLogicalSize(PySDL_Renderer *self, PyObject *args) {
    int w, h;
    int mode = SDL_LOGICAL_PRESENTATION_LETTERBOX;
    if(!PyArg_ParseTuple(args, "ii|i", &w, &h, &mode)) {
        return NULL;
    }
    if(!SDL_SetRenderLogicalPresentation(self->renderer, w, h, (SDL_RendererLogicalPresentation)mode)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_GetLogicalSize(PySDL_Renderer *self, PyObject *ign) {
    int w = 0, h = 0;
    SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    SDL_GetRenderLogicalPresentation(self->renderer, &w, &h, &mode);
    return Py_BuildValue("(iii)", w, h, mode);
}

//=========================================================
// render targets
//=========================================================

static PyObject * PySDL_Renderer_SetRenderTarget(PySDL_Renderer *self, PyObject *arg) {
    SDL_Texture *texture = NULL;
    if(arg != Py_None) {
        if(!PyObject_TypeCheck(arg, &PySDL_Texture_Type)) {
            PyErr_SetString(PyExc_TypeError, "expected an SDL3.Texture or None");
            return NULL;
        }
        texture = ((PySDL_Texture *)arg)->texture;
    }

    if(!SDL_SetRenderTarget(self->renderer, texture)) {
        return _raise();
    }

    if(arg == Py_None) {
        Py_CLEAR(self->target);
    } else {
        Py_INCREF(arg);
        Py_XSETREF(self->target, arg);
    }

    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_GetRenderTarget(PySDL_Renderer *self, PyObject *ign) {
    SDL_Texture *texture = SDL_GetRenderTarget(self->renderer);
    if(NULL == texture) {
        Py_RETURN_NONE;
    }
    if(NULL != self->target && ((PySDL_Texture *)self->target)->texture == texture) {
        return Py_NewRef(self->target);
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Renderer_ReadPixels(PySDL_Renderer *self, PyObject *args, PyObject *kwds) {
    PyObject *rect_py = NULL;
    unsigned int format = SDL_PIXELFORMAT_ARGB8888;

    static char *kwlist[] = {"rect", "format", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|OI", kwlist, &rect_py, &format)) {
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

    // SDL3 returns a freshly allocated surface in an arbitrary format.
    SDL_Surface *pixels = SDL_RenderReadPixels(self->renderer, rp);
    if(NULL == pixels) {
        return _raise();
    }

    SDL_Surface *out = pixels;
    if((SDL_PixelFormat)format != pixels->format) {
        out = SDL_ConvertSurface(pixels, (SDL_PixelFormat)format);
        SDL_DestroySurface(pixels);
        if(NULL == out) {
            return _raise();
        }
    }

    PyObject *buffer = PyBytes_FromStringAndSize(out->pixels, (Py_ssize_t)out->pitch * out->h);
    SDL_DestroySurface(out);
    return buffer;
}

//=========================================================
// info
//=========================================================

static PyObject * PySDL_Renderer_GetRendererInfo(PySDL_Renderer *self, PyObject *ign) {
    // SDL3 dropped SDL_RendererInfo; name + a few renderer properties replace it.
    const char *name = SDL_GetRendererName(self->renderer);
    SDL_PropertiesID props = SDL_GetRendererProperties(self->renderer);

    PyObject *formats = PyList_New(0);
    if(NULL == formats) {
        return NULL;
    }
    const SDL_PixelFormat *fmts = (const SDL_PixelFormat *)SDL_GetPointerProperty(
        props, SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, NULL);
    for(int i = 0; fmts && fmts[i] != SDL_PIXELFORMAT_UNKNOWN; ++i) {
        PyObject *v = PyLong_FromUnsignedLong(fmts[i]);
        if(NULL == v || 0 != PyList_Append(formats, v)) {
            Py_XDECREF(v);
            Py_DECREF(formats);
            return NULL;
        }
        Py_DECREF(v);
    }

    Sint64 maxsize = SDL_GetNumberProperty(props, SDL_PROP_RENDERER_MAX_TEXTURE_SIZE_NUMBER, 0);
    return Py_BuildValue("(sNii)", name ? name : "", formats, (int)maxsize, (int)maxsize);
}

static PyObject * PySDL_Renderer_GetRendererOutputSize(PySDL_Renderer *self, PyObject *ign) {
    int w = 0, h = 0;
    if(!SDL_GetRenderOutputSize(self->renderer, &w, &h)) {
        return _raise();
    }
    return Py_BuildValue("(ii)", w, h);
}
