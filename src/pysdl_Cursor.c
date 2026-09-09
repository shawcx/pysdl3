#include "pysdl.h"

static int        PySDL_Cursor_Type_init    (PySDL_Cursor*, PyObject*, PyObject*);
static void       PySDL_Cursor_Type_dealloc (PySDL_Cursor*);

static PyObject * PySDL_Cursor_Set (PySDL_Cursor*, PyObject*);

static PyMethodDef PySDL_Cursor_methods[] = {
    { "Set", (PyCFunction)PySDL_Cursor_Set, METH_NOARGS },
    { NULL }
};

PyTypeObject PySDL_Cursor_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Cursor",
    .tp_basicsize = sizeof(PySDL_Cursor),
    .tp_dealloc   = (destructor)PySDL_Cursor_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Cursor(system_cursor_id)",
    .tp_methods   = PySDL_Cursor_methods,
    .tp_init      = (initproc)PySDL_Cursor_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_Cursor_Type_init(PySDL_Cursor *self, PyObject *args, PyObject *kwds) {
    int system_id = -1;  // SDL_SYSTEM_CURSOR_ARROW is 0, so -1 means "not given"
    if(!PyArg_ParseTuple(args, "|i", &system_id)) {
        return -1;
    }

    self->cursor = NULL;
    self->shouldFree = 1;

    if(system_id >= 0) {
        self->cursor = SDL_CreateSystemCursor((SDL_SystemCursor)system_id);
        if(NULL == self->cursor) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_Cursor_Type_dealloc(PySDL_Cursor *self) {
    if(NULL != self->cursor && self->shouldFree) {
        SDL_DestroyCursor(self->cursor);
    }
    self->cursor = NULL;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * PySDL_Cursor_Set(PySDL_Cursor *self, PyObject *ign) {
    if(NULL == self->cursor) {
        PyErr_SetString(pysdl_Error, "Cursor is not initialized");
        return NULL;
    }
    SDL_SetCursor(self->cursor);
    Py_RETURN_NONE;
}

// Wrap an SDL_Cursor; `borrowed` cursors (GetCursor / GetDefaultCursor) are not
// freed on dealloc.
static PyObject * _wrap(SDL_Cursor *cursor, int borrowed) {
    if(NULL == cursor) {
        if(!borrowed) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return NULL;
        }
        Py_RETURN_NONE;
    }
    PySDL_Cursor *wrapper = (PySDL_Cursor *)PySDL_New(&PySDL_Cursor_Type);
    if(NULL == wrapper) {
        if(!borrowed) {
            SDL_DestroyCursor(cursor);
        }
        return NULL;
    }
    wrapper->cursor = cursor;
    wrapper->shouldFree = !borrowed;
    return (PyObject *)wrapper;
}

//=========================================================
// module functions
//=========================================================

static PyObject * PySDL_CreateColorCursor(PyObject *self, PyObject *args) {
    PyObject *surface_py;
    int hot_x = 0;
    int hot_y = 0;
    if(!PyArg_ParseTuple(args, "O|ii", &surface_py, &hot_x, &hot_y)) {
        return NULL;
    }
    if(!PyObject_TypeCheck(surface_py, &PySDL_Surface_Type)) {
        PyErr_SetString(PyExc_TypeError, "expected an SDL2.Surface");
        return NULL;
    }
    return _wrap(SDL_CreateColorCursor(((PySDL_Surface *)surface_py)->surface, hot_x, hot_y), 0);
}

static PyObject * PySDL_CreateCursor(PyObject *self, PyObject *args) {
    Py_buffer data, mask;
    int w, h, hot_x, hot_y;
    if(!PyArg_ParseTuple(args, "y*y*(ii)(ii)", &data, &mask, &w, &h, &hot_x, &hot_y)) {
        return NULL;
    }

    Py_ssize_t need = (Py_ssize_t)((w + 7) / 8) * h;
    if(data.len < need || mask.len < need) {
        PyBuffer_Release(&data);
        PyBuffer_Release(&mask);
        PyErr_Format(PyExc_ValueError, "data and mask must be at least %zd bytes for a %dx%d cursor", need, w, h);
        return NULL;
    }

    SDL_Cursor *cursor = SDL_CreateCursor(data.buf, mask.buf, w, h, hot_x, hot_y);
    PyBuffer_Release(&data);
    PyBuffer_Release(&mask);
    return _wrap(cursor, 0);
}

static PyObject * PySDL_GetCursor(PyObject *self, PyObject *ign) {
    return _wrap(SDL_GetCursor(), 1);
}

static PyObject * PySDL_GetDefaultCursor(PyObject *self, PyObject *ign) {
    return _wrap(SDL_GetDefaultCursor(), 1);
}

static PyObject * PySDL_SetCursor(PyObject *self, PyObject *arg) {
    SDL_Cursor *cursor = NULL;
    if(arg != Py_None) {
        if(!PyObject_TypeCheck(arg, &PySDL_Cursor_Type)) {
            PyErr_SetString(PyExc_TypeError, "expected an SDL2.Cursor or None");
            return NULL;
        }
        cursor = ((PySDL_Cursor *)arg)->cursor;
    }
    SDL_SetCursor(cursor);
    Py_RETURN_NONE;
}

PyMethodDef pysdl_cursor_methods[] = {
    { "CreateColorCursor", PySDL_CreateColorCursor, METH_VARARGS },
    { "CreateCursor",      PySDL_CreateCursor,      METH_VARARGS },
    { "GetCursor",         PySDL_GetCursor,         METH_NOARGS  },
    { "GetDefaultCursor",  PySDL_GetDefaultCursor,  METH_NOARGS  },
    { "SetCursor",         PySDL_SetCursor,         METH_O       },
    { NULL }
};
