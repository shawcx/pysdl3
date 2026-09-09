#include "pysdl.h"

static int        PySDL_Timer_Type_init    (PySDL_Timer*, PyObject*, PyObject*);
static void       PySDL_Timer_Type_dealloc (PySDL_Timer*);

static PyObject * PySDL_Timer_Remove (PySDL_Timer*, PyObject*);

static PyMethodDef PySDL_Timer_methods[] = {
    { "Remove", (PyCFunction)PySDL_Timer_Remove, METH_NOARGS },
    { NULL }
};

static PyObject * PySDL_Timer_getid(PyObject *self, void *ign) {
    (void)ign;
    return PyLong_FromLong(((PySDL_Timer *)self)->id);
}

static PyGetSetDef PySDL_Timer_getset[] = {
    { "id",     PySDL_Timer_getid, NULL, "SDL_TimerID (0 once removed)", NULL },
    { NULL }
};

PyTypeObject PySDL_Timer_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Timer",
    .tp_basicsize = sizeof(PySDL_Timer),
    .tp_dealloc   = (destructor)PySDL_Timer_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Timer(interval_ms, callback) - callback(interval) returns the "
                    "next interval, or None to repeat, or 0 to stop",
    .tp_methods   = PySDL_Timer_methods,
    .tp_getset    = PySDL_Timer_getset,
    .tp_init      = (initproc)PySDL_Timer_Type_init,
    .tp_new       = PyType_GenericNew
};

static Uint32 SDLCALL _timer_callback(Uint32 interval, void *param) {
    PyObject *callable = (PyObject *)param;
    PyGILState_STATE gil;

    if(!PySDL_ThreadEnter(&gil)) {
        return 0;  // interpreter gone: stop the timer
    }

    Uint32 next = 0;
    PyObject *arg = PyLong_FromUnsignedLong(interval);
    PyObject *result = PyObject_CallFunctionObjArgs(callable, arg, NULL);
    Py_XDECREF(arg);

    if(NULL == result) {
        PyErr_Print();
    } else {
        if(result == Py_None) {
            next = interval;  // repeat at the same rate
        } else {
            long value = PyLong_AsLong(result);
            if(-1 == value && PyErr_Occurred()) {
                PyErr_Print();
            } else if(value > 0) {
                next = (Uint32)value;
            }
        }
        Py_DECREF(result);
    }

    PySDL_ThreadLeave(gil);
    return next;
}

static int PySDL_Timer_Type_init(PySDL_Timer *self, PyObject *args, PyObject *kwds) {
    unsigned int interval = 0;
    PyObject *callback = NULL;

    static char *kwlist[] = {"interval", "callback", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|IO", kwlist, &interval, &callback)) {
        return -1;
    }

    self->id = 0;
    self->callback = NULL;

    if(NULL != callback) {
        if(!PyCallable_Check(callback)) {
            PyErr_SetString(PyExc_TypeError, "callback must be callable");
            return -1;
        }
        Py_INCREF(callback);
        self->id = SDL_AddTimer(interval, _timer_callback, callback);
        if(0 == self->id) {
            Py_DECREF(callback);
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
        self->callback = callback;  // one ref, shared with SDL's param
    }
    return 0;
}

static void _remove(PySDL_Timer *self) {
    if(0 != self->id) {
        // Drop the GIL: SDL_RemoveTimer waits for the callback thread, which
        // needs the GIL.
        SDL_TimerID id = self->id;
        Py_BEGIN_ALLOW_THREADS
            SDL_RemoveTimer(id);
        Py_END_ALLOW_THREADS
        self->id = 0;
    }
    Py_CLEAR(self->callback);
}

static void PySDL_Timer_Type_dealloc(PySDL_Timer *self) {
    _remove(self);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * PySDL_Timer_Remove(PySDL_Timer *self, PyObject *ign) {
    _remove(self);
    Py_RETURN_NONE;
}
