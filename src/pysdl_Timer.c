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

// SDL_RemoveTimer does not wait for a callback already under way: it only
// marks the timer cancelled. So SDL is never given a pointer to a Python
// object (it could be freed while the timer thread waits for the GIL).
// Instead each timer gets an integer token; SDL's `param` is the token, and
// the callback - holding the GIL - looks it up in `_timers` (token ->
// callable). Remove() deletes the entry under the GIL, so a late callback
// finds nothing and stops the timer.
static PyObject *_timers;            // dict: token -> callable
static unsigned long long _next_token;

static PyObject * _token_key(void *param) {
    return PyLong_FromUnsignedLongLong((unsigned long long)(uintptr_t)param);
}

static Uint32 SDLCALL _timer_callback(void *param, SDL_TimerID timerID, Uint32 interval) {
    (void)timerID;
    PyGILState_STATE gil;

    if(!PySDL_ThreadEnter(&gil)) {
        return 0;  // interpreter gone: stop the timer
    }

    Uint32 next = 0;
    PyObject *key = _token_key(param);
    PyObject *callable = (key && _timers) ? PyDict_GetItemWithError(_timers, key) : NULL;
    Py_XDECREF(key);
    if(NULL == callable) {
        if(PyErr_Occurred()) {
            PyErr_Print();
        }
        PySDL_ThreadLeave(gil);
        return 0;  // removed while this call was pending
    }

    Py_INCREF(callable);  // Remove() may run from inside the callback
    PyObject *arg = PyLong_FromUnsignedLong(interval);
    PyObject *result = arg ? PyObject_CallFunctionObjArgs(callable, arg, NULL) : NULL;
    Py_XDECREF(arg);
    Py_DECREF(callable);

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
    self->token = 0;

    if(NULL != callback) {
        if(!PyCallable_Check(callback)) {
            PyErr_SetString(PyExc_TypeError, "callback must be callable");
            return -1;
        }
        if(NULL == _timers && NULL == (_timers = PyDict_New())) {
            return -1;
        }
        unsigned long long token = ++_next_token;
        PyObject *key = PyLong_FromUnsignedLongLong(token);
        if(NULL == key || 0 > PyDict_SetItem(_timers, key, callback)) {
            Py_XDECREF(key);
            return -1;
        }
        self->id = SDL_AddTimer(interval, _timer_callback, (void *)(uintptr_t)token);
        if(0 == self->id) {
            PyDict_DelItem(_timers, key);
            Py_DECREF(key);
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
        Py_DECREF(key);
        self->token = token;
    }
    return 0;
}

static void _remove(PySDL_Timer *self) {
    if(0 != self->token && NULL != _timers) {
        // First, under the GIL: a callback still pending now finds no entry.
        PyObject *key = PyLong_FromUnsignedLongLong(self->token);
        if(NULL == key || 0 > PyDict_DelItem(_timers, key)) {
            PyErr_Clear();  // already gone (e.g. dealloc after Remove)
        }
        Py_XDECREF(key);
        self->token = 0;
    }
    if(0 != self->id) {
        SDL_TimerID id = self->id;
        self->id = 0;
        Py_BEGIN_ALLOW_THREADS
            SDL_RemoveTimer(id);
        Py_END_ALLOW_THREADS
    }
}

static void PySDL_Timer_Type_dealloc(PySDL_Timer *self) {
    _remove(self);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * PySDL_Timer_Remove(PySDL_Timer *self, PyObject *ign) {
    _remove(self);
    Py_RETURN_NONE;
}
