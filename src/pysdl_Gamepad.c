#include "pysdl.h"

// SDL3 renamed SDL_GameController -> SDL_Gamepad throughout. The Python type is
// SDL3.Gamepad (with a GameController alias); opened by SDL_JoystickID.

static int        PySDL_Gamepad_Type_init    (PySDL_Gamepad*, PyObject*, PyObject*);
static void       PySDL_Gamepad_Type_dealloc (PySDL_Gamepad*);

static PyObject * PySDL_Gamepad_Name           (PySDL_Gamepad*, PyObject*);
static PyObject * PySDL_Gamepad_Attached       (PySDL_Gamepad*, PyObject*);
static PyObject * PySDL_Gamepad_GetAxis        (PySDL_Gamepad*, PyObject*);
static PyObject * PySDL_Gamepad_GetButton      (PySDL_Gamepad*, PyObject*);
static PyObject * PySDL_Gamepad_Mapping        (PySDL_Gamepad*, PyObject*);
static PyObject * PySDL_Gamepad_GetJoystick    (PySDL_Gamepad*, PyObject*);
static PyObject * PySDL_Gamepad_Rumble         (PySDL_Gamepad*, PyObject*);
static PyObject * PySDL_Gamepad_RumbleTriggers (PySDL_Gamepad*, PyObject*);
static PyObject * PySDL_Gamepad_SetLED         (PySDL_Gamepad*, PyObject*);
static PyObject * PySDL_Gamepad_GetType        (PySDL_Gamepad*, PyObject*);
static PyObject * PySDL_Gamepad_Close          (PySDL_Gamepad*, PyObject*);

static PyMethodDef PySDL_Gamepad_methods[] = {
    { "Name",           (PyCFunction)PySDL_Gamepad_Name,           METH_NOARGS },
    { "Attached",       (PyCFunction)PySDL_Gamepad_Attached,       METH_NOARGS },
    { "GetAxis",        (PyCFunction)PySDL_Gamepad_GetAxis,        METH_O      },
    { "GetButton",      (PyCFunction)PySDL_Gamepad_GetButton,      METH_O      },
    { "Mapping",        (PyCFunction)PySDL_Gamepad_Mapping,        METH_NOARGS },
    { "GetJoystick",    (PyCFunction)PySDL_Gamepad_GetJoystick,    METH_NOARGS },
    { "Rumble",         (PyCFunction)PySDL_Gamepad_Rumble,         METH_VARARGS },
    { "RumbleTriggers", (PyCFunction)PySDL_Gamepad_RumbleTriggers, METH_VARARGS },
    { "SetLED",         (PyCFunction)PySDL_Gamepad_SetLED,         METH_VARARGS },
    { "GetType",        (PyCFunction)PySDL_Gamepad_GetType,        METH_NOARGS },
    { "Close",          (PyCFunction)PySDL_Gamepad_Close,          METH_NOARGS },
    { NULL }
};

PyTypeObject PySDL_Gamepad_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Gamepad",
    .tp_basicsize = sizeof(PySDL_Gamepad),
    .tp_dealloc   = (destructor)PySDL_Gamepad_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Gamepad(instance_id)",
    .tp_methods   = PySDL_Gamepad_methods,
    .tp_init      = (initproc)PySDL_Gamepad_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_Gamepad_Type_init(PySDL_Gamepad *self, PyObject *args, PyObject *kwds) {
    long long id = -1;
    if(!PyArg_ParseTuple(args, "|L", &id)) {
        return -1;
    }

    self->gamepad = NULL;

    if(id > 0) {
        self->gamepad = SDL_OpenGamepad((SDL_JoystickID)id);
        if(NULL == self->gamepad) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_Gamepad_Type_dealloc(PySDL_Gamepad *self) {
    if(NULL != self->gamepad) {
        SDL_CloseGamepad(self->gamepad);
        self->gamepad = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static SDL_Gamepad * _gp(PySDL_Gamepad *self) {
    if(NULL == self->gamepad) {
        PyErr_SetString(pysdl_Error, "Gamepad is not open");
        return NULL;
    }
    return self->gamepad;
}

static PyObject * PySDL_Gamepad_Name(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp) return NULL;
    const char *name = SDL_GetGamepadName(gp);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_Gamepad_Attached(PySDL_Gamepad *self, PyObject *ign) {
    return PyBool_FromLong(self->gamepad && SDL_GamepadConnected(self->gamepad));
}

static PyObject * PySDL_Gamepad_GetAxis(PySDL_Gamepad *self, PyObject *arg) {
    SDL_Gamepad *gp = _gp(self);
    long axis = PyLong_AsLong(arg);
    if(NULL == gp || (-1 == axis && PyErr_Occurred())) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetGamepadAxis(gp, (SDL_GamepadAxis)axis));
}

static PyObject * PySDL_Gamepad_GetButton(PySDL_Gamepad *self, PyObject *arg) {
    SDL_Gamepad *gp = _gp(self);
    long button = PyLong_AsLong(arg);
    if(NULL == gp || (-1 == button && PyErr_Occurred())) {
        return NULL;
    }
    return PyBool_FromLong(SDL_GetGamepadButton(gp, (SDL_GamepadButton)button));
}

static PyObject * PySDL_Gamepad_Mapping(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp) return NULL;
    char *mapping = SDL_GetGamepadMapping(gp);
    if(NULL == mapping) {
        Py_RETURN_NONE;
    }
    PyObject *result = PyUnicode_FromString(mapping);
    SDL_free(mapping);
    return result;
}

static PyObject * PySDL_Gamepad_GetJoystick(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp) return NULL;

    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gp);
    if(NULL == joystick) {
        Py_RETURN_NONE;
    }
    PySDL_Joystick *wrapper = (PySDL_Joystick *)PySDL_New(&PySDL_Joystick_Type);
    if(NULL == wrapper) {
        return NULL;
    }
    wrapper->joystick = joystick;  // owned by the gamepad
    wrapper->shouldFree = 0;
    return (PyObject *)wrapper;
}

static PyObject * PySDL_Gamepad_Rumble(PySDL_Gamepad *self, PyObject *args) {
    unsigned int low, high, ms;
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp || !PyArg_ParseTuple(args, "III", &low, &high, &ms)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_RumbleGamepad(gp, (Uint16)low, (Uint16)high, ms));
}

static PyObject * PySDL_Gamepad_RumbleTriggers(PySDL_Gamepad *self, PyObject *args) {
    unsigned int left, right, ms;
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp || !PyArg_ParseTuple(args, "III", &left, &right, &ms)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_RumbleGamepadTriggers(gp, (Uint16)left, (Uint16)right, ms));
}

static PyObject * PySDL_Gamepad_SetLED(PySDL_Gamepad *self, PyObject *args) {
    unsigned char r, g, b;
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp || !PyArg_ParseTuple(args, "bbb", &r, &g, &b)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_SetGamepadLED(gp, r, g, b));
}

static PyObject * PySDL_Gamepad_GetType(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromLong(SDL_GetGamepadType(gp)) : NULL;
}

static PyObject * PySDL_Gamepad_Close(PySDL_Gamepad *self, PyObject *ign) {
    if(NULL != self->gamepad) {
        SDL_CloseGamepad(self->gamepad);
        self->gamepad = NULL;
    }
    Py_RETURN_NONE;
}

//=========================================================
// module functions
//=========================================================

static PyObject * PySDL_IsGameController(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    return PyBool_FromLong(SDL_IsGamepad((SDL_JoystickID)id));
}

static PyObject * PySDL_GameControllerNameForIndex(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_GetGamepadNameForID((SDL_JoystickID)id);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_GameControllerAddMapping(PyObject *self, PyObject *arg) {
    const char *mapping = PyUnicode_AsUTF8(arg);
    if(NULL == mapping) {
        return NULL;
    }
    int rc = SDL_AddGamepadMapping(mapping);
    if(-1 == rc) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyLong_FromLong(rc);  // 1 added, 0 updated
}

static PyObject * PySDL_GameControllerAddMappingsFromFile(PyObject *self, PyObject *arg) {
    const char *path = PyUnicode_AsUTF8(arg);
    if(NULL == path) {
        return NULL;
    }
    int rc = SDL_AddGamepadMappingsFromFile(path);
    if(-1 == rc) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyLong_FromLong(rc);
}

static PyObject * PySDL_GameControllerUpdate(PyObject *self, PyObject *ign) {
    SDL_UpdateGamepads();
    Py_RETURN_NONE;
}

static PyObject * PySDL_GameControllerEventState(PyObject *self, PyObject *args) {
    PyObject *state = NULL;
    if(!PyArg_ParseTuple(args, "|O", &state)) {
        return NULL;
    }
    if(state && state != Py_None) {
        SDL_SetGamepadEventsEnabled(PyObject_IsTrue(state) ? true : false);
    }
    return PyBool_FromLong(SDL_GamepadEventsEnabled());
}

PyMethodDef pysdl_gamepad_methods[] = {
    { "IsGameController",                 PySDL_IsGameController,                 METH_O       },
    { "IsGamepad",                        PySDL_IsGameController,                 METH_O       },
    { "GameControllerNameForIndex",       PySDL_GameControllerNameForIndex,       METH_O       },
    { "GetGamepadNameForID",              PySDL_GameControllerNameForIndex,       METH_O       },
    { "GameControllerAddMapping",         PySDL_GameControllerAddMapping,         METH_O       },
    { "AddGamepadMapping",                PySDL_GameControllerAddMapping,         METH_O       },
    { "GameControllerAddMappingsFromFile", PySDL_GameControllerAddMappingsFromFile, METH_O     },
    { "AddGamepadMappingsFromFile",      PySDL_GameControllerAddMappingsFromFile, METH_O      },
    { "GameControllerUpdate",             PySDL_GameControllerUpdate,             METH_NOARGS  },
    { "UpdateGamepads",                   PySDL_GameControllerUpdate,             METH_NOARGS  },
    { "GameControllerEventState",         PySDL_GameControllerEventState,         METH_VARARGS },
    { "SetGamepadEventsEnabled",          PySDL_GameControllerEventState,         METH_VARARGS },
    { NULL }
};
