#include "pysdl.h"

static int        PySDL_Joystick_Type_init    (PySDL_Joystick*, PyObject*, PyObject*);
static void       PySDL_Joystick_Type_dealloc (PySDL_Joystick*);

static PyObject * PySDL_Joystick_Name              (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetGUID           (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_InstanceID        (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_Attached          (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_NumAxes           (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_NumButtons        (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_NumHats           (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_NumBalls          (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetAxis           (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetButton         (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetHat            (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetBall           (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_Rumble            (PySDL_Joystick*, PyObject*);
#if SDL_VERSION_ATLEAST(2,0,14)
static PyObject * PySDL_Joystick_RumbleTriggers    (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetLED            (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_HasLED            (PySDL_Joystick*, PyObject*);
#endif
static PyObject * PySDL_Joystick_CurrentPowerLevel (PySDL_Joystick*, PyObject*);
#if SDL_VERSION_ATLEAST(2,0,14)
static PyObject * PySDL_Joystick_SetVirtualAxis   (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetVirtualButton (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetVirtualHat    (PySDL_Joystick*, PyObject*);
#endif
static PyObject * PySDL_Joystick_Close             (PySDL_Joystick*, PyObject*);

static PyMethodDef PySDL_Joystick_methods[] = {
    { "Name",              (PyCFunction)PySDL_Joystick_Name,              METH_NOARGS },
    { "GetGUID",           (PyCFunction)PySDL_Joystick_GetGUID,           METH_NOARGS },
    { "InstanceID",        (PyCFunction)PySDL_Joystick_InstanceID,        METH_NOARGS },
    { "Attached",          (PyCFunction)PySDL_Joystick_Attached,          METH_NOARGS },
    { "NumAxes",           (PyCFunction)PySDL_Joystick_NumAxes,           METH_NOARGS },
    { "NumButtons",        (PyCFunction)PySDL_Joystick_NumButtons,        METH_NOARGS },
    { "NumHats",           (PyCFunction)PySDL_Joystick_NumHats,           METH_NOARGS },
    { "NumBalls",          (PyCFunction)PySDL_Joystick_NumBalls,          METH_NOARGS },
    { "GetAxis",           (PyCFunction)PySDL_Joystick_GetAxis,           METH_O      },
    { "GetButton",         (PyCFunction)PySDL_Joystick_GetButton,         METH_O      },
    { "GetHat",            (PyCFunction)PySDL_Joystick_GetHat,            METH_O      },
    { "GetBall",           (PyCFunction)PySDL_Joystick_GetBall,           METH_O      },
    { "Rumble",            (PyCFunction)PySDL_Joystick_Rumble,            METH_VARARGS },
#if SDL_VERSION_ATLEAST(2,0,14)
    { "RumbleTriggers",    (PyCFunction)PySDL_Joystick_RumbleTriggers,    METH_VARARGS },
    { "SetLED",            (PyCFunction)PySDL_Joystick_SetLED,            METH_VARARGS },
    { "HasLED",            (PyCFunction)PySDL_Joystick_HasLED,            METH_NOARGS },
#endif
    { "CurrentPowerLevel", (PyCFunction)PySDL_Joystick_CurrentPowerLevel, METH_NOARGS },
#if SDL_VERSION_ATLEAST(2,0,14)
    { "SetVirtualAxis",    (PyCFunction)PySDL_Joystick_SetVirtualAxis,    METH_VARARGS },
    { "SetVirtualButton",  (PyCFunction)PySDL_Joystick_SetVirtualButton,  METH_VARARGS },
    { "SetVirtualHat",     (PyCFunction)PySDL_Joystick_SetVirtualHat,     METH_VARARGS },
#endif
    { "Close",             (PyCFunction)PySDL_Joystick_Close,             METH_NOARGS },
    { NULL }
};

PyTypeObject PySDL_Joystick_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Joystick",
    .tp_basicsize = sizeof(PySDL_Joystick),
    .tp_dealloc   = (destructor)PySDL_Joystick_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Joystick(device_index)",
    .tp_methods   = PySDL_Joystick_methods,
    .tp_init      = (initproc)PySDL_Joystick_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_Joystick_Type_init(PySDL_Joystick *self, PyObject *args, PyObject *kwds) {
    int index = -1;
    if(!PyArg_ParseTuple(args, "|i", &index)) {
        return -1;
    }

    self->joystick = NULL;
    self->shouldFree = 1;

    if(index >= 0) {
        self->joystick = SDL_JoystickOpen(index);
        if(NULL == self->joystick) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_Joystick_Type_dealloc(PySDL_Joystick *self) {
    if(NULL != self->joystick && self->shouldFree) {
        SDL_JoystickClose(self->joystick);
    }
    self->joystick = NULL;
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static SDL_Joystick * _js(PySDL_Joystick *self) {
    if(NULL == self->joystick) {
        PyErr_SetString(pysdl_Error, "Joystick is not open");
        return NULL;
    }
    return self->joystick;
}

static PyObject * PySDL_Joystick_Name(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    if(NULL == js) return NULL;
    const char *name = SDL_JoystickName(js);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_Joystick_GetGUID(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    if(NULL == js) return NULL;
    char buffer[33];
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(js), buffer, sizeof(buffer));
    return PyUnicode_FromString(buffer);
}

static PyObject * PySDL_Joystick_InstanceID(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    if(NULL == js) return NULL;
    return PyLong_FromLong(SDL_JoystickInstanceID(js));
}

static PyObject * PySDL_Joystick_Attached(PySDL_Joystick *self, PyObject *ign) {
    return PyBool_FromLong(self->joystick && SDL_JoystickGetAttached(self->joystick));
}

static PyObject * PySDL_Joystick_NumAxes(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_JoystickNumAxes(js)) : NULL;
}

static PyObject * PySDL_Joystick_NumButtons(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_JoystickNumButtons(js)) : NULL;
}

static PyObject * PySDL_Joystick_NumHats(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_JoystickNumHats(js)) : NULL;
}

static PyObject * PySDL_Joystick_NumBalls(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_JoystickNumBalls(js)) : NULL;
}

static PyObject * PySDL_Joystick_GetAxis(PySDL_Joystick *self, PyObject *arg) {
    SDL_Joystick *js = _js(self);
    long axis = PyLong_AsLong(arg);
    if(NULL == js || (-1 == axis && PyErr_Occurred())) {
        return NULL;
    }
    return PyLong_FromLong(SDL_JoystickGetAxis(js, (int)axis));
}

static PyObject * PySDL_Joystick_GetButton(PySDL_Joystick *self, PyObject *arg) {
    SDL_Joystick *js = _js(self);
    long button = PyLong_AsLong(arg);
    if(NULL == js || (-1 == button && PyErr_Occurred())) {
        return NULL;
    }
    return PyBool_FromLong(SDL_JoystickGetButton(js, (int)button));
}

static PyObject * PySDL_Joystick_GetHat(PySDL_Joystick *self, PyObject *arg) {
    SDL_Joystick *js = _js(self);
    long hat = PyLong_AsLong(arg);
    if(NULL == js || (-1 == hat && PyErr_Occurred())) {
        return NULL;
    }
    return PyLong_FromLong(SDL_JoystickGetHat(js, (int)hat));
}

static PyObject * PySDL_Joystick_GetBall(PySDL_Joystick *self, PyObject *arg) {
    SDL_Joystick *js = _js(self);
    long ball = PyLong_AsLong(arg);
    if(NULL == js || (-1 == ball && PyErr_Occurred())) {
        return NULL;
    }
    int dx = 0, dy = 0;
    if(0 > SDL_JoystickGetBall(js, (int)ball, &dx, &dy)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return Py_BuildValue("(ii)", dx, dy);
}

static PyObject * PySDL_Joystick_Rumble(PySDL_Joystick *self, PyObject *args) {
    unsigned int low, high, ms;
    SDL_Joystick *js = _js(self);
    if(NULL == js || !PyArg_ParseTuple(args, "III", &low, &high, &ms)) {
        return NULL;
    }
    return PyBool_FromLong(0 == SDL_JoystickRumble(js, (Uint16)low, (Uint16)high, ms));
}

#if SDL_VERSION_ATLEAST(2,0,14)
static PyObject * PySDL_Joystick_RumbleTriggers(PySDL_Joystick *self, PyObject *args) {
    unsigned int left, right, ms;
    SDL_Joystick *js = _js(self);
    if(NULL == js || !PyArg_ParseTuple(args, "III", &left, &right, &ms)) {
        return NULL;
    }
    return PyBool_FromLong(0 == SDL_JoystickRumbleTriggers(js, (Uint16)left, (Uint16)right, ms));
}

static PyObject * PySDL_Joystick_SetLED(PySDL_Joystick *self, PyObject *args) {
    unsigned char r, g, b;
    SDL_Joystick *js = _js(self);
    if(NULL == js || !PyArg_ParseTuple(args, "bbb", &r, &g, &b)) {
        return NULL;
    }
    return PyBool_FromLong(0 == SDL_JoystickSetLED(js, r, g, b));
}

static PyObject * PySDL_Joystick_HasLED(PySDL_Joystick *self, PyObject *ign) {
    return PyBool_FromLong(self->joystick && SDL_JoystickHasLED(self->joystick));
}
#endif

static PyObject * PySDL_Joystick_CurrentPowerLevel(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_JoystickCurrentPowerLevel(js)) : NULL;
}

#if SDL_VERSION_ATLEAST(2,0,14)
static PyObject * PySDL_Joystick_SetVirtualAxis(PySDL_Joystick *self, PyObject *args) {
    int axis, value;
    SDL_Joystick *js = _js(self);
    if(NULL == js || !PyArg_ParseTuple(args, "ii", &axis, &value)) {
        return NULL;
    }
    if(0 > SDL_JoystickSetVirtualAxis(js, axis, (Sint16)value)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Joystick_SetVirtualButton(PySDL_Joystick *self, PyObject *args) {
    int button, value;
    SDL_Joystick *js = _js(self);
    if(NULL == js || !PyArg_ParseTuple(args, "ii", &button, &value)) {
        return NULL;
    }
    if(0 > SDL_JoystickSetVirtualButton(js, button, (Uint8)value)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Joystick_SetVirtualHat(PySDL_Joystick *self, PyObject *args) {
    int hat, value;
    SDL_Joystick *js = _js(self);
    if(NULL == js || !PyArg_ParseTuple(args, "ii", &hat, &value)) {
        return NULL;
    }
    if(0 > SDL_JoystickSetVirtualHat(js, hat, (Uint8)value)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}
#endif

static PyObject * PySDL_Joystick_Close(PySDL_Joystick *self, PyObject *ign) {
    if(NULL != self->joystick && self->shouldFree) {
        SDL_JoystickClose(self->joystick);
    }
    self->joystick = NULL;
    Py_RETURN_NONE;
}

//=========================================================
// module functions
//=========================================================

static PyObject * PySDL_NumJoysticks(PyObject *self, PyObject *ign) {
    return PyLong_FromLong(SDL_NumJoysticks());
}

static PyObject * PySDL_JoystickNameForIndex(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_JoystickNameForIndex((int)index);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_JoystickUpdate(PyObject *self, PyObject *ign) {
    SDL_JoystickUpdate();
    Py_RETURN_NONE;
}

static PyObject * PySDL_JoystickEventState(PyObject *self, PyObject *args) {
    int state = SDL_QUERY;
    if(!PyArg_ParseTuple(args, "|i", &state)) {
        return NULL;
    }
    return PyLong_FromLong(SDL_JoystickEventState(state));
}

#if SDL_VERSION_ATLEAST(2,0,14)
static PyObject * PySDL_JoystickAttachVirtual(PyObject *self, PyObject *args) {
    int type, naxes, nbuttons, nhats;
    if(!PyArg_ParseTuple(args, "iiii", &type, &naxes, &nbuttons, &nhats)) {
        return NULL;
    }
    int index = SDL_JoystickAttachVirtual((SDL_JoystickType)type, naxes, nbuttons, nhats);
    if(0 > index) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyLong_FromLong(index);
}

static PyObject * PySDL_JoystickDetachVirtual(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    if(0 > SDL_JoystickDetachVirtual((int)index)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_JoystickIsVirtual(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    return PyBool_FromLong(SDL_JoystickIsVirtual((int)index));
}
#endif

PyMethodDef pysdl_joystick_methods[] = {
    { "NumJoysticks",           PySDL_NumJoysticks,           METH_NOARGS  },
    { "JoystickNameForIndex",   PySDL_JoystickNameForIndex,   METH_O       },
    { "JoystickUpdate",         PySDL_JoystickUpdate,         METH_NOARGS  },
    { "JoystickEventState",     PySDL_JoystickEventState,     METH_VARARGS },
#if SDL_VERSION_ATLEAST(2,0,14)
    { "JoystickAttachVirtual",  PySDL_JoystickAttachVirtual,  METH_VARARGS },
    { "JoystickDetachVirtual",  PySDL_JoystickDetachVirtual,  METH_O       },
    { "JoystickIsVirtual",      PySDL_JoystickIsVirtual,      METH_O       },
#endif
    { NULL }
};
