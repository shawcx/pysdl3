#include "pysdl.h"

static int        PySDL_Palette_Type_init    (PySDL_Palette*, PyObject*, PyObject*);
static void       PySDL_Palette_Type_dealloc (PySDL_Palette*);

static PyObject * PySDL_Palette_SetColors (PySDL_Palette*, PyObject*, PyObject*);
static PyObject * PySDL_Palette_GetColors (PySDL_Palette*, PyObject*);

static PyMethodDef PySDL_Palette_methods[] = {
    { "SetColors", (PyCFunction)PySDL_Palette_SetColors, METH_VARARGS | METH_KEYWORDS },
    { "GetColors", (PyCFunction)PySDL_Palette_GetColors, METH_NOARGS  },
    { NULL }
};

static SDL_Palette * _pal(PySDL_Palette *self) {
    if(NULL == self->palette) {
        PyErr_SetString(pysdl_Error, "Palette is not initialized");
        return NULL;
    }
    return self->palette;
}

static PyObject * PySDL_Palette_getter(PyObject *self, void *param) {
    SDL_Palette *palette = _pal((PySDL_Palette *)self);
    (void)param;
    if(NULL == palette) {
        return NULL;
    }
    return PyLong_FromLong(palette->ncolors);
}

static PyGetSetDef PySDL_Palette_getset[] = {
    { "ncolors", PySDL_Palette_getter, NULL, "number of entries", NULL },
    { NULL }
};

PyTypeObject PySDL_Palette_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Palette",
    .tp_basicsize = sizeof(PySDL_Palette),
    .tp_dealloc   = (destructor)PySDL_Palette_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Palette(ncolors)",
    .tp_methods   = PySDL_Palette_methods,
    .tp_getset    = PySDL_Palette_getset,
    .tp_init      = (initproc)PySDL_Palette_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_Palette_Type_init(PySDL_Palette *self, PyObject *args, PyObject *kwds) {
    int ncolors = 0;
    if(!PyArg_ParseTuple(args, "|i", &ncolors)) {
        return -1;
    }

    self->palette = NULL;
    if(ncolors > 0) {
        self->palette = SDL_CreatePalette(ncolors);
        if(NULL == self->palette) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_Palette_Type_dealloc(PySDL_Palette *self) {
    if(NULL != self->palette) {
        SDL_DestroyPalette(self->palette);
        self->palette = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * PySDL_Palette_SetColors(PySDL_Palette *self, PyObject *args, PyObject *kwds) {
    PyObject *colors_py;
    int first = 0;

    static char *kwlist[] = {"colors", "first", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|i", kwlist, &colors_py, &first)) {
        return NULL;
    }

    SDL_Palette *palette = _pal(self);
    if(NULL == palette) {
        return NULL;
    }

    PyObject *fast = PySequence_Fast(colors_py, "expected a list of (r, g, b[, a]) colors");
    if(NULL == fast) {
        return NULL;
    }

    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    SDL_Color *colors = PyMem_New(SDL_Color, n > 0 ? n : 1);
    if(NULL == colors) {
        Py_DECREF(fast);
        return PyErr_NoMemory();
    }
    for(Py_ssize_t idx = 0; idx < n; ++idx) {
        if(!PyToColor(PySequence_Fast_GET_ITEM(fast, idx), &colors[idx])) {
            PyMem_Free(colors);
            Py_DECREF(fast);
            return NULL;
        }
    }
    Py_DECREF(fast);

    bool ok = SDL_SetPaletteColors(palette, colors, first, (int)n);
    PyMem_Free(colors);
    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Palette_GetColors(PySDL_Palette *self, PyObject *ign) {
    SDL_Palette *palette = _pal(self);
    if(NULL == palette) {
        return NULL;
    }
    int n = palette->ncolors;
    PyObject *list = PyList_New(n);
    if(NULL == list) {
        return NULL;
    }
    for(int idx = 0; idx < n; ++idx) {
        SDL_Color *c = &palette->colors[idx];
        PyObject *tuple = Py_BuildValue("(iiii)", c->r, c->g, c->b, c->a);
        if(NULL == tuple) {
            Py_DECREF(list);
            return NULL;
        }
        PyList_SET_ITEM(list, idx, tuple);
    }
    return list;
}
