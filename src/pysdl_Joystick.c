#include "pysdl.h"

// SDL3: joysticks are opened by SDL_JoystickID (from SDL_GetJoysticks), not a
// device index.

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
static PyObject * PySDL_Joystick_RumbleTriggers    (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetLED            (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_HasLED            (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_CurrentPowerLevel (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetVirtualAxis   (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetVirtualButton (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetVirtualHat    (PySDL_Joystick*, PyObject*);
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
    { "RumbleTriggers",    (PyCFunction)PySDL_Joystick_RumbleTriggers,    METH_VARARGS },
    { "SetLED",            (PyCFunction)PySDL_Joystick_SetLED,            METH_VARARGS },
    { "HasLED",            (PyCFunction)PySDL_Joystick_HasLED,            METH_NOARGS },
    { "CurrentPowerLevel", (PyCFunction)PySDL_Joystick_CurrentPowerLevel, METH_NOARGS },
    { "SetVirtualAxis",    (PyCFunction)PySDL_Joystick_SetVirtualAxis,    METH_VARARGS },
    { "SetVirtualButton",  (PyCFunction)PySDL_Joystick_SetVirtualButton,  METH_VARARGS },
    { "SetVirtualHat",     (PyCFunction)PySDL_Joystick_SetVirtualHat,     METH_VARARGS },
    { "Close",             (PyCFunction)PySDL_Joystick_Close,             METH_NOARGS },
    { NULL }
};

PyTypeObject PySDL_Joystick_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Joystick",
    .tp_basicsize = sizeof(PySDL_Joystick),
    .tp_dealloc   = (destructor)PySDL_Joystick_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Joystick(instance_id)",
    .tp_methods   = PySDL_Joystick_methods,
    .tp_init      = (initproc)PySDL_Joystick_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_Joystick_Type_init(PySDL_Joystick *self, PyObject *args, PyObject *kwds) {
    long long id = -1;
    if(!PyArg_ParseTuple(args, "|L", &id)) {
        return -1;
    }

    self->joystick = NULL;
    self->shouldFree = 1;

    if(id > 0) {
        self->joystick = SDL_OpenJoystick((SDL_JoystickID)id);
        if(NULL == self->joystick) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_Joystick_Type_dealloc(PySDL_Joystick *self) {
    if(NULL != self->joystick && self->shouldFree) {
        SDL_CloseJoystick(self->joystick);
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
    const char *name = SDL_GetJoystickName(js);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_Joystick_GetGUID(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    if(NULL == js) return NULL;
    char buffer[33];
    SDL_GUIDToString(SDL_GetJoystickGUID(js), buffer, sizeof(buffer));
    return PyUnicode_FromString(buffer);
}

static PyObject * PySDL_Joystick_InstanceID(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    if(NULL == js) return NULL;
    return PyLong_FromUnsignedLong(SDL_GetJoystickID(js));
}

static PyObject * PySDL_Joystick_Attached(PySDL_Joystick *self, PyObject *ign) {
    return PyBool_FromLong(self->joystick && SDL_JoystickConnected(self->joystick));
}

static PyObject * PySDL_Joystick_NumAxes(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_GetNumJoystickAxes(js)) : NULL;
}

static PyObject * PySDL_Joystick_NumButtons(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_GetNumJoystickButtons(js)) : NULL;
}

static PyObject * PySDL_Joystick_NumHats(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_GetNumJoystickHats(js)) : NULL;
}

static PyObject * PySDL_Joystick_NumBalls(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_GetNumJoystickBalls(js)) : NULL;
}

static PyObject * PySDL_Joystick_GetAxis(PySDL_Joystick *self, PyObject *arg) {
    SDL_Joystick *js = _js(self);
    long axis = PyLong_AsLong(arg);
    if(NULL == js || (-1 == axis && PyErr_Occurred())) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetJoystickAxis(js, (int)axis));
}

static PyObject * PySDL_Joystick_GetButton(PySDL_Joystick *self, PyObject *arg) {
    SDL_Joystick *js = _js(self);
    long button = PyLong_AsLong(arg);
    if(NULL == js || (-1 == button && PyErr_Occurred())) {
        return NULL;
    }
    return PyBool_FromLong(SDL_GetJoystickButton(js, (int)button));
}

static PyObject * PySDL_Joystick_GetHat(PySDL_Joystick *self, PyObject *arg) {
    SDL_Joystick *js = _js(self);
    long hat = PyLong_AsLong(arg);
    if(NULL == js || (-1 == hat && PyErr_Occurred())) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetJoystickHat(js, (int)hat));
}

static PyObject * PySDL_Joystick_GetBall(PySDL_Joystick *self, PyObject *arg) {
    SDL_Joystick *js = _js(self);
    long ball = PyLong_AsLong(arg);
    if(NULL == js || (-1 == ball && PyErr_Occurred())) {
        return NULL;
    }
    int dx = 0, dy = 0;
    if(!SDL_GetJoystickBall(js, (int)ball, &dx, &dy)) {
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
    return PyBool_FromLong(SDL_RumbleJoystick(js, (Uint16)low, (Uint16)high, ms));
}

static PyObject * PySDL_Joystick_RumbleTriggers(PySDL_Joystick *self, PyObject *args) {
    unsigned int left, right, ms;
    SDL_Joystick *js = _js(self);
    if(NULL == js || !PyArg_ParseTuple(args, "III", &left, &right, &ms)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_RumbleJoystickTriggers(js, (Uint16)left, (Uint16)right, ms));
}

static PyObject * PySDL_Joystick_SetLED(PySDL_Joystick *self, PyObject *args) {
    unsigned char r, g, b;
    SDL_Joystick *js = _js(self);
    if(NULL == js || !PyArg_ParseTuple(args, "bbb", &r, &g, &b)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_SetJoystickLED(js, r, g, b));
}

static PyObject * PySDL_Joystick_HasLED(PySDL_Joystick *self, PyObject *ign) {
    if(NULL == self->joystick) {
        Py_RETURN_FALSE;
    }
    SDL_PropertiesID props = SDL_GetJoystickProperties(self->joystick);
    return PyBool_FromLong(SDL_GetBooleanProperty(props, SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN, false));
}

static PyObject * PySDL_Joystick_CurrentPowerLevel(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    if(NULL == js) return NULL;
    int percent = -1;
    return PyLong_FromLong(SDL_GetJoystickPowerInfo(js, &percent));
}

static PyObject * PySDL_Joystick_SetVirtualAxis(PySDL_Joystick *self, PyObject *args) {
    int axis, value;
    SDL_Joystick *js = _js(self);
    if(NULL == js || !PyArg_ParseTuple(args, "ii", &axis, &value)) {
        return NULL;
    }
    if(!SDL_SetJoystickVirtualAxis(js, axis, (Sint16)value)) {
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
    if(!SDL_SetJoystickVirtualButton(js, button, value ? true : false)) {
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
    if(!SDL_SetJoystickVirtualHat(js, hat, (Uint8)value)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Joystick_Close(PySDL_Joystick *self, PyObject *ign) {
    if(NULL != self->joystick && self->shouldFree) {
        SDL_CloseJoystick(self->joystick);
    }
    self->joystick = NULL;
    Py_RETURN_NONE;
}

//=========================================================
// module functions
//=========================================================

static PyObject * PySDL_NumJoysticks(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&count);
    SDL_free(ids);
    return PyLong_FromLong(count);
}

static PyObject * PySDL_GetJoysticks(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_JoystickID *ids = SDL_GetJoysticks(&count);
    PyObject *list = PyList_New(count);
    if(NULL == list) {
        SDL_free(ids);
        return NULL;
    }
    for(int idx = 0; idx < count; ++idx) {
        PyList_SET_ITEM(list, idx, PyLong_FromUnsignedLong(ids[idx]));
    }
    SDL_free(ids);
    return list;
}

static PyObject * PySDL_JoystickNameForIndex(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_GetJoystickNameForID((SDL_JoystickID)id);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_JoystickUpdate(PyObject *self, PyObject *ign) {
    SDL_UpdateJoysticks();
    Py_RETURN_NONE;
}

static PyObject * PySDL_JoystickEventState(PyObject *self, PyObject *args) {
    PyObject *state = NULL;
    if(!PyArg_ParseTuple(args, "|O", &state)) {
        return NULL;
    }
    if(state && state != Py_None) {
        SDL_SetJoystickEventsEnabled(PyObject_IsTrue(state) ? true : false);
    }
    return PyBool_FromLong(SDL_JoystickEventsEnabled());
}

static PyObject * PySDL_JoystickAttachVirtual(PyObject *self, PyObject *args) {
    int type, naxes, nbuttons, nhats;
    if(!PyArg_ParseTuple(args, "iiii", &type, &naxes, &nbuttons, &nhats)) {
        return NULL;
    }
    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type = (Uint16)type;
    desc.naxes = (Uint16)naxes;
    desc.nbuttons = (Uint16)nbuttons;
    desc.nhats = (Uint16)nhats;

    SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    if(0 == id) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyLong_FromUnsignedLong(id);
}

static PyObject * PySDL_JoystickDetachVirtual(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    if(!SDL_DetachVirtualJoystick((SDL_JoystickID)id)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_JoystickIsVirtual(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    return PyBool_FromLong(SDL_IsJoystickVirtual((SDL_JoystickID)id));
}

PyMethodDef pysdl_joystick_methods[] = {
    { "NumJoysticks",           PySDL_NumJoysticks,           METH_NOARGS  },
    { "GetJoysticks",           PySDL_GetJoysticks,           METH_NOARGS  },
    { "JoystickNameForIndex",   PySDL_JoystickNameForIndex,   METH_O       },
    { "GetJoystickNameForID",   PySDL_JoystickNameForIndex,   METH_O       },
    { "JoystickUpdate",         PySDL_JoystickUpdate,         METH_NOARGS  },
    { "JoystickEventState",     PySDL_JoystickEventState,     METH_VARARGS },
    { "JoystickAttachVirtual",  PySDL_JoystickAttachVirtual,  METH_VARARGS },
    { "JoystickDetachVirtual",  PySDL_JoystickDetachVirtual,  METH_O       },
    { "JoystickIsVirtual",      PySDL_JoystickIsVirtual,      METH_O       },
    { NULL }
};
