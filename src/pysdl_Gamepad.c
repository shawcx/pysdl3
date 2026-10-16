#include "pysdl.h"

// SDL3 renamed SDL_GameController -> SDL_Gamepad throughout. The Python type is
// SDL3.Gamepad (with a GameController alias); opened by SDL_JoystickID. SDL
// refcounts opens of the same ID, so lookups return new owned wrappers.
// Every Python entry point runs holding SDL's joystick lock, taken without the
// GIL (PYSDL_JOYSTICK_LOCKED; see pysdl_Joystick.c).

static int        PySDL_Gamepad_Type_init    (PySDL_Gamepad*, PyObject*, PyObject*);
static void       PySDL_Gamepad_Type_dealloc (PySDL_Gamepad*);

PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_Name,                           PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_InstanceID,                     PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_Attached,                       PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetAxis,                        PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetButton,                      PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_HasAxis,                        PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_HasButton,                      PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetButtonLabel,                 PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_Mapping,                        PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetBindings,                    PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetBindForAxis,                 PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetBindForButton,               PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetJoystick,                    PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_Rumble,                         PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_RumbleTriggers,                 PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_SetLED,                         PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_HasLED,                         PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_HasRumble,                      PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_HasRumbleTriggers,              PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_SendEffect,                     PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetType,                        PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetRealType,                    PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetVendor,                      PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetProduct,                     PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetProductVersion,              PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetFirmwareVersion,             PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetSerial,                      PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_Path,                           PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetSteamHandle,                 PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetPlayerIndex,                 PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_SetPlayerIndex,                 PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetPowerInfo,                   PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetConnectionState,             PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_HasSensor,                      PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_SetSensorEnabled,               PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_IsSensorEnabled,                PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetSensorDataRate,              PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED_KW(PySDL_Gamepad_GetSensorData,               PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetNumTouchpads,                PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetNumTouchpadFingers,          PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetTouchpadFinger,              PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetAppleSFSymbolsNameForButton, PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_GetAppleSFSymbolsNameForAxis,   PySDL_Gamepad *)
PYSDL_JOYSTICK_LOCKED(PySDL_Gamepad_Close,                          PySDL_Gamepad *)

static PyMethodDef PySDL_Gamepad_methods[] = {
    { "Name",              (PyCFunction)PySDL_Gamepad_Name,              METH_NOARGS },
    { "InstanceID",        (PyCFunction)PySDL_Gamepad_InstanceID,        METH_NOARGS },
    { "Attached",          (PyCFunction)PySDL_Gamepad_Attached,          METH_NOARGS },
    { "GetAxis",           (PyCFunction)PySDL_Gamepad_GetAxis,           METH_O      },
    { "GetButton",         (PyCFunction)PySDL_Gamepad_GetButton,         METH_O      },
    { "HasAxis",           (PyCFunction)PySDL_Gamepad_HasAxis,           METH_O      },
    { "HasButton",         (PyCFunction)PySDL_Gamepad_HasButton,         METH_O      },
    { "GetButtonLabel",    (PyCFunction)PySDL_Gamepad_GetButtonLabel,    METH_O      },
    { "Mapping",           (PyCFunction)PySDL_Gamepad_Mapping,           METH_NOARGS },
    { "GetBindings",       (PyCFunction)PySDL_Gamepad_GetBindings,       METH_NOARGS },
    { "GetBindForAxis",    (PyCFunction)PySDL_Gamepad_GetBindForAxis,    METH_O      },
    { "GetBindForButton",  (PyCFunction)PySDL_Gamepad_GetBindForButton,  METH_O      },
    { "GetJoystick",       (PyCFunction)PySDL_Gamepad_GetJoystick,       METH_NOARGS },
    { "Rumble",            (PyCFunction)PySDL_Gamepad_Rumble,            METH_VARARGS },
    { "RumbleTriggers",    (PyCFunction)PySDL_Gamepad_RumbleTriggers,    METH_VARARGS },
    { "SetLED",            (PyCFunction)PySDL_Gamepad_SetLED,            METH_VARARGS },
    { "HasLED",            (PyCFunction)PySDL_Gamepad_HasLED,            METH_NOARGS },
    { "HasRumble",         (PyCFunction)PySDL_Gamepad_HasRumble,         METH_NOARGS },
    { "HasRumbleTriggers", (PyCFunction)PySDL_Gamepad_HasRumbleTriggers, METH_NOARGS },
    { "SendEffect",        (PyCFunction)PySDL_Gamepad_SendEffect,        METH_O      },
    { "GetType",           (PyCFunction)PySDL_Gamepad_GetType,           METH_NOARGS },
    { "GetRealType",       (PyCFunction)PySDL_Gamepad_GetRealType,       METH_NOARGS },
    { "GetVendor",         (PyCFunction)PySDL_Gamepad_GetVendor,         METH_NOARGS },
    { "GetProduct",        (PyCFunction)PySDL_Gamepad_GetProduct,        METH_NOARGS },
    { "GetProductVersion", (PyCFunction)PySDL_Gamepad_GetProductVersion, METH_NOARGS },
    { "GetFirmwareVersion", (PyCFunction)PySDL_Gamepad_GetFirmwareVersion, METH_NOARGS },
    { "GetSerial",         (PyCFunction)PySDL_Gamepad_GetSerial,         METH_NOARGS },
    { "Path",              (PyCFunction)PySDL_Gamepad_Path,              METH_NOARGS },
    { "GetSteamHandle",    (PyCFunction)PySDL_Gamepad_GetSteamHandle,    METH_NOARGS },
    { "GetPlayerIndex",    (PyCFunction)PySDL_Gamepad_GetPlayerIndex,    METH_NOARGS },
    { "SetPlayerIndex",    (PyCFunction)PySDL_Gamepad_SetPlayerIndex,    METH_O      },
    { "GetPowerInfo",      (PyCFunction)PySDL_Gamepad_GetPowerInfo,      METH_NOARGS },
    { "GetConnectionState", (PyCFunction)PySDL_Gamepad_GetConnectionState, METH_NOARGS },
    { "HasSensor",         (PyCFunction)PySDL_Gamepad_HasSensor,         METH_O      },
    { "SetSensorEnabled",  (PyCFunction)PySDL_Gamepad_SetSensorEnabled,  METH_VARARGS },
    { "IsSensorEnabled",   (PyCFunction)PySDL_Gamepad_IsSensorEnabled,   METH_O      },
    { "GetSensorDataRate", (PyCFunction)PySDL_Gamepad_GetSensorDataRate, METH_O      },
    { "GetSensorData",     (PyCFunction)PySDL_Gamepad_GetSensorData,     METH_VARARGS | METH_KEYWORDS },
    { "GetNumTouchpads",   (PyCFunction)PySDL_Gamepad_GetNumTouchpads,   METH_NOARGS },
    { "GetNumTouchpadFingers", (PyCFunction)PySDL_Gamepad_GetNumTouchpadFingers, METH_O },
    { "GetTouchpadFinger", (PyCFunction)PySDL_Gamepad_GetTouchpadFinger, METH_VARARGS },
    { "GetAppleSFSymbolsNameForButton", (PyCFunction)PySDL_Gamepad_GetAppleSFSymbolsNameForButton, METH_O },
    { "GetAppleSFSymbolsNameForAxis",   (PyCFunction)PySDL_Gamepad_GetAppleSFSymbolsNameForAxis,   METH_O },
    { "Close",             (PyCFunction)PySDL_Gamepad_Close,             METH_NOARGS },
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
        int locked = PySDL_JoystickLock();
        self->gamepad = SDL_OpenGamepad((SDL_JoystickID)id);
        PySDL_JoystickUnlock(locked);
        if(NULL == self->gamepad) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_Gamepad_Type_dealloc(PySDL_Gamepad *self) {
    if(NULL != self->gamepad) {
        int locked = PySDL_JoystickLock();
        SDL_CloseGamepad(self->gamepad);
        PySDL_JoystickUnlock(locked);
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

// Parse a METH_O int argument alongside the open-gamepad guard.
static SDL_Gamepad * _gp_int(PySDL_Gamepad *self, PyObject *arg, int *out) {
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp) {
        return NULL;
    }
    long value = PyLong_AsLong(arg);
    if(-1 == value && PyErr_Occurred()) {
        return NULL;
    }
    *out = (int)value;
    return gp;
}

// A const char* SDL may return as NULL -> str or None.
static PyObject * _str_or_none(const char *text) {
    if(NULL == text) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(text);
}

// An SDL_malloc'd mapping string -> str (freed) or None.
static PyObject * _take_mapping(char *mapping) {
    if(NULL == mapping) {
        SDL_ClearError();
        Py_RETURN_NONE;
    }
    PyObject *result = PyUnicode_FromString(mapping);
    SDL_free(mapping);
    return result;
}

static PyObject * _cap(SDL_Gamepad *gp, const char *name) {
    if(NULL == gp) {
        Py_RETURN_FALSE;
    }
    return PyBool_FromLong(SDL_GetBooleanProperty(SDL_GetGamepadProperties(gp), name, false));
}

static PyObject * PySDL_Gamepad_Name_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp) return NULL;
    const char *name = SDL_GetGamepadName(gp);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_Gamepad_InstanceID_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromUnsignedLong(SDL_GetGamepadID(gp)) : NULL;
}

static PyObject * PySDL_Gamepad_Attached_impl(PySDL_Gamepad *self, PyObject *ign) {
    return PyBool_FromLong(self->gamepad && SDL_GamepadConnected(self->gamepad));
}

static PyObject * PySDL_Gamepad_GetAxis_impl(PySDL_Gamepad *self, PyObject *arg) {
    int axis;
    SDL_Gamepad *gp = _gp_int(self, arg, &axis);
    return gp ? PyLong_FromLong(SDL_GetGamepadAxis(gp, (SDL_GamepadAxis)axis)) : NULL;
}

static PyObject * PySDL_Gamepad_GetButton_impl(PySDL_Gamepad *self, PyObject *arg) {
    int button;
    SDL_Gamepad *gp = _gp_int(self, arg, &button);
    return gp ? PyBool_FromLong(SDL_GetGamepadButton(gp, (SDL_GamepadButton)button)) : NULL;
}

static PyObject * PySDL_Gamepad_HasAxis_impl(PySDL_Gamepad *self, PyObject *arg) {
    int axis;
    SDL_Gamepad *gp = _gp_int(self, arg, &axis);
    return gp ? PyBool_FromLong(SDL_GamepadHasAxis(gp, (SDL_GamepadAxis)axis)) : NULL;
}

static PyObject * PySDL_Gamepad_HasButton_impl(PySDL_Gamepad *self, PyObject *arg) {
    int button;
    SDL_Gamepad *gp = _gp_int(self, arg, &button);
    return gp ? PyBool_FromLong(SDL_GamepadHasButton(gp, (SDL_GamepadButton)button)) : NULL;
}

// -> GAMEPAD_BUTTON_LABEL_* (what is printed on the button: A, Cross, ...)
static PyObject * PySDL_Gamepad_GetButtonLabel_impl(PySDL_Gamepad *self, PyObject *arg) {
    int button;
    SDL_Gamepad *gp = _gp_int(self, arg, &button);
    return gp ? PyLong_FromLong(SDL_GetGamepadButtonLabel(gp, (SDL_GamepadButton)button)) : NULL;
}

static PyObject * PySDL_Gamepad_Mapping_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? _take_mapping(SDL_GetGamepadMapping(gp)) : NULL;
}

//---------------------------------------------------------
// bindings (SDL3 replaced GetBindForAxis/Button with SDL_GetGamepadBindings)
//---------------------------------------------------------

// The joystick side of a binding: (BINDTYPE_BUTTON, button),
// (BINDTYPE_AXIS, axis, axis_min, axis_max), (BINDTYPE_HAT, hat, hat_mask) or None.
static PyObject * _bind_input(const SDL_GamepadBinding *b) {
    switch(b->input_type) {
    case SDL_GAMEPAD_BINDTYPE_BUTTON:
        return Py_BuildValue("(ii)", b->input_type, b->input.button);
    case SDL_GAMEPAD_BINDTYPE_AXIS:
        return Py_BuildValue("(iiii)", b->input_type, b->input.axis.axis,
            b->input.axis.axis_min, b->input.axis.axis_max);
    case SDL_GAMEPAD_BINDTYPE_HAT:
        return Py_BuildValue("(iii)", b->input_type, b->input.hat.hat, b->input.hat.hat_mask);
    default:
        Py_RETURN_NONE;
    }
}

// The gamepad side: (BINDTYPE_BUTTON, GAMEPAD_BUTTON_*),
// (BINDTYPE_AXIS, GAMEPAD_AXIS_*, axis_min, axis_max) or None.
static PyObject * _bind_output(const SDL_GamepadBinding *b) {
    switch(b->output_type) {
    case SDL_GAMEPAD_BINDTYPE_BUTTON:
        return Py_BuildValue("(ii)", b->output_type, b->output.button);
    case SDL_GAMEPAD_BINDTYPE_AXIS:
        return Py_BuildValue("(iiii)", b->output_type, b->output.axis.axis,
            b->output.axis.axis_min, b->output.axis.axis_max);
    default:
        Py_RETURN_NONE;
    }
}

static SDL_GamepadBinding ** _bindings(PySDL_Gamepad *self, int *count) {
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp) {
        return NULL;
    }
    SDL_GamepadBinding **bindings = SDL_GetGamepadBindings(gp, count);
    if(NULL == bindings) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
    }
    return bindings;
}

// GetBindings() -> [(input, output), ...] (see _bind_input / _bind_output)
static PyObject * PySDL_Gamepad_GetBindings_impl(PySDL_Gamepad *self, PyObject *ign) {
    int count = 0;
    SDL_GamepadBinding **bindings = _bindings(self, &count);
    if(NULL == bindings) {
        return NULL;
    }
    PyObject *list = PyList_New(count);
    for(int idx = 0; list && idx < count; ++idx) {
        PyObject *item = Py_BuildValue("(NN)", _bind_input(bindings[idx]), _bind_output(bindings[idx]));
        if(NULL == item) {
            Py_CLEAR(list);
            break;
        }
        PyList_SET_ITEM(list, idx, item);
    }
    SDL_free(bindings);
    return list;
}

// The SDL2-style answer for the first binding that drives `value` on the
// gamepad side: None, (BINDTYPE_BUTTON, button), (BINDTYPE_AXIS, axis) or
// (BINDTYPE_HAT, hat, hat_mask).
static PyObject * _bind_for(PySDL_Gamepad *self, PyObject *arg, SDL_GamepadBindingType kind) {
    long value = PyLong_AsLong(arg);
    if(-1 == value && PyErr_Occurred()) {
        return NULL;
    }
    int count = 0;
    SDL_GamepadBinding **bindings = _bindings(self, &count);
    if(NULL == bindings) {
        return NULL;
    }
    PyObject *result = NULL;
    for(int idx = 0; NULL == result && idx < count; ++idx) {
        const SDL_GamepadBinding *b = bindings[idx];
        if(b->output_type != kind) {
            continue;
        }
        int target = (SDL_GAMEPAD_BINDTYPE_BUTTON == kind) ? (int)b->output.button : (int)b->output.axis.axis;
        if(target != value) {
            continue;
        }
        switch(b->input_type) {
        case SDL_GAMEPAD_BINDTYPE_BUTTON:
            result = Py_BuildValue("(ii)", b->input_type, b->input.button);
            break;
        case SDL_GAMEPAD_BINDTYPE_AXIS:
            result = Py_BuildValue("(ii)", b->input_type, b->input.axis.axis);
            break;
        case SDL_GAMEPAD_BINDTYPE_HAT:
            result = Py_BuildValue("(iii)", b->input_type, b->input.hat.hat, b->input.hat.hat_mask);
            break;
        default:
            break;
        }
        if(NULL == result && PyErr_Occurred()) {
            break;
        }
    }
    SDL_free(bindings);
    if(NULL == result && !PyErr_Occurred()) {
        Py_RETURN_NONE;
    }
    return result;
}

static PyObject * PySDL_Gamepad_GetBindForAxis_impl(PySDL_Gamepad *self, PyObject *arg) {
    return _bind_for(self, arg, SDL_GAMEPAD_BINDTYPE_AXIS);
}

static PyObject * PySDL_Gamepad_GetBindForButton_impl(PySDL_Gamepad *self, PyObject *arg) {
    return _bind_for(self, arg, SDL_GAMEPAD_BINDTYPE_BUTTON);
}

// The gamepad's joystick as an owned Joystick: re-opening it by ID bumps SDL's
// refcount, so it stays valid after the gamepad is closed (a borrowed pointer
// would dangle). None if the device has gone.
static PyObject * PySDL_Gamepad_GetJoystick_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp) return NULL;

    SDL_Joystick *joystick = SDL_GetGamepadJoystick(gp);
    if(NULL == joystick) {
        SDL_ClearError();
        Py_RETURN_NONE;
    }
    return PyObject_CallFunction((PyObject *)&PySDL_Joystick_Type, "k", (unsigned long)SDL_GetJoystickID(joystick));
}

static PyObject * PySDL_Gamepad_Rumble_impl(PySDL_Gamepad *self, PyObject *args) {
    unsigned int low, high, ms;
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp || !PyArg_ParseTuple(args, "III", &low, &high, &ms)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_RumbleGamepad(gp, (Uint16)low, (Uint16)high, ms));
}

static PyObject * PySDL_Gamepad_RumbleTriggers_impl(PySDL_Gamepad *self, PyObject *args) {
    unsigned int left, right, ms;
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp || !PyArg_ParseTuple(args, "III", &left, &right, &ms)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_RumbleGamepadTriggers(gp, (Uint16)left, (Uint16)right, ms));
}

static PyObject * PySDL_Gamepad_SetLED_impl(PySDL_Gamepad *self, PyObject *args) {
    unsigned char r, g, b;
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp || !PyArg_ParseTuple(args, "bbb", &r, &g, &b)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_SetGamepadLED(gp, r, g, b));
}

static PyObject * PySDL_Gamepad_HasLED_impl(PySDL_Gamepad *self, PyObject *ign) {
    return _cap(self->gamepad, SDL_PROP_GAMEPAD_CAP_RGB_LED_BOOLEAN);
}

static PyObject * PySDL_Gamepad_HasRumble_impl(PySDL_Gamepad *self, PyObject *ign) {
    return _cap(self->gamepad, SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN);
}

static PyObject * PySDL_Gamepad_HasRumbleTriggers_impl(PySDL_Gamepad *self, PyObject *ign) {
    return _cap(self->gamepad, SDL_PROP_GAMEPAD_CAP_TRIGGER_RUMBLE_BOOLEAN);
}

// SendEffect(data: bytes-like) -> bool
static PyObject * PySDL_Gamepad_SendEffect_impl(PySDL_Gamepad *self, PyObject *arg) {
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp) return NULL;
    Py_buffer data;
    if(0 > PyObject_GetBuffer(arg, &data, PyBUF_SIMPLE)) {
        return NULL;
    }
    if(data.len > INT_MAX) {
        PyBuffer_Release(&data);
        PyErr_SetString(PyExc_ValueError, "effect data too large");
        return NULL;
    }
    bool ok = SDL_SendGamepadEffect(gp, data.buf, (int)data.len);
    PyBuffer_Release(&data);
    return PyBool_FromLong(ok);
}

static PyObject * PySDL_Gamepad_GetType_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromLong(SDL_GetGamepadType(gp)) : NULL;
}

// The type ignoring any mapping override.
static PyObject * PySDL_Gamepad_GetRealType_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromLong(SDL_GetRealGamepadType(gp)) : NULL;
}

static PyObject * PySDL_Gamepad_GetVendor_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromLong(SDL_GetGamepadVendor(gp)) : NULL;
}

static PyObject * PySDL_Gamepad_GetProduct_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromLong(SDL_GetGamepadProduct(gp)) : NULL;
}

static PyObject * PySDL_Gamepad_GetProductVersion_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromLong(SDL_GetGamepadProductVersion(gp)) : NULL;
}

static PyObject * PySDL_Gamepad_GetFirmwareVersion_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromLong(SDL_GetGamepadFirmwareVersion(gp)) : NULL;
}

static PyObject * PySDL_Gamepad_GetSerial_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? _str_or_none(SDL_GetGamepadSerial(gp)) : NULL;
}

static PyObject * PySDL_Gamepad_Path_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? _str_or_none(SDL_GetGamepadPath(gp)) : NULL;
}

// The Steam Input handle (an InputHandle_t), or 0 when not driven by Steam.
static PyObject * PySDL_Gamepad_GetSteamHandle_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromUnsignedLongLong(SDL_GetGamepadSteamHandle(gp)) : NULL;
}

static PyObject * PySDL_Gamepad_GetPlayerIndex_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromLong(SDL_GetGamepadPlayerIndex(gp)) : NULL;
}

// SetPlayerIndex(index) - -1 clears it.
static PyObject * PySDL_Gamepad_SetPlayerIndex_impl(PySDL_Gamepad *self, PyObject *arg) {
    int player;
    SDL_Gamepad *gp = _gp_int(self, arg, &player);
    if(NULL == gp) return NULL;
    if(!SDL_SetGamepadPlayerIndex(gp, player)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

// -> (POWERSTATE_*, percent or -1)
static PyObject * PySDL_Gamepad_GetPowerInfo_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp) return NULL;
    int percent = -1;
    SDL_PowerState state = SDL_GetGamepadPowerInfo(gp, &percent);
    return Py_BuildValue("(ii)", state, percent);
}

// -> JOYSTICK_CONNECTION_*
static PyObject * PySDL_Gamepad_GetConnectionState_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromLong(SDL_GetGamepadConnectionState(gp)) : NULL;
}

//---------------------------------------------------------
// sensors / touchpads
//---------------------------------------------------------

static PyObject * PySDL_Gamepad_HasSensor_impl(PySDL_Gamepad *self, PyObject *arg) {
    int type;
    SDL_Gamepad *gp = _gp_int(self, arg, &type);
    return gp ? PyBool_FromLong(SDL_GamepadHasSensor(gp, (SDL_SensorType)type)) : NULL;
}

// SetSensorEnabled(type, enabled) - raises if the gamepad has no such sensor.
static PyObject * PySDL_Gamepad_SetSensorEnabled_impl(PySDL_Gamepad *self, PyObject *args) {
    int type, enabled;
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp || !PyArg_ParseTuple(args, "ip", &type, &enabled)) {
        return NULL;
    }
    if(!SDL_SetGamepadSensorEnabled(gp, (SDL_SensorType)type, enabled ? true : false)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Gamepad_IsSensorEnabled_impl(PySDL_Gamepad *self, PyObject *arg) {
    int type;
    SDL_Gamepad *gp = _gp_int(self, arg, &type);
    return gp ? PyBool_FromLong(SDL_GamepadSensorEnabled(gp, (SDL_SensorType)type)) : NULL;
}

// -> Hz, or 0.0 when unknown / no such sensor.
static PyObject * PySDL_Gamepad_GetSensorDataRate_impl(PySDL_Gamepad *self, PyObject *arg) {
    int type;
    SDL_Gamepad *gp = _gp_int(self, arg, &type);
    return gp ? PyFloat_FromDouble(SDL_GetGamepadSensorDataRate(gp, (SDL_SensorType)type)) : NULL;
}

// GetSensorData(type, count=3) -> tuple of `count` floats (SDL keeps 3 per
// gamepad sensor; any beyond are 0.0).
static PyObject * PySDL_Gamepad_GetSensorData_impl(PySDL_Gamepad *self, PyObject *args, PyObject *kwds) {
    int type, count = 3;
    SDL_Gamepad *gp = _gp(self);
    static char *kwlist[] = {"type", "count", NULL};
    if(NULL == gp || !PyArg_ParseTupleAndKeywords(args, kwds, "i|i", kwlist, &type, &count)) {
        return NULL;
    }
    if(count < 1 || count > 16) {
        PyErr_SetString(PyExc_ValueError, "count must be 1..16");
        return NULL;
    }

    float values[16] = { 0 };
    if(!SDL_GetGamepadSensorData(gp, (SDL_SensorType)type, values, count)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    PyObject *tuple = PyTuple_New(count);
    if(NULL == tuple) {
        return NULL;
    }
    for(int idx = 0; idx < count; ++idx) {
        PyObject *f = PyFloat_FromDouble(values[idx]);
        if(NULL == f) {
            Py_DECREF(tuple);
            return NULL;
        }
        PyTuple_SET_ITEM(tuple, idx, f);
    }
    return tuple;
}

static PyObject * PySDL_Gamepad_GetNumTouchpads_impl(PySDL_Gamepad *self, PyObject *ign) {
    SDL_Gamepad *gp = _gp(self);
    return gp ? PyLong_FromLong(SDL_GetNumGamepadTouchpads(gp)) : NULL;
}

// -> simultaneous fingers on that touchpad (0 for a touchpad that isn't there)
static PyObject * PySDL_Gamepad_GetNumTouchpadFingers_impl(PySDL_Gamepad *self, PyObject *arg) {
    int touchpad;
    SDL_Gamepad *gp = _gp_int(self, arg, &touchpad);
    return gp ? PyLong_FromLong(SDL_GetNumGamepadTouchpadFingers(gp, touchpad)) : NULL;
}

// GetTouchpadFinger(touchpad, finger) -> (down, x, y, pressure)
static PyObject * PySDL_Gamepad_GetTouchpadFinger_impl(PySDL_Gamepad *self, PyObject *args) {
    int touchpad, finger;
    SDL_Gamepad *gp = _gp(self);
    if(NULL == gp || !PyArg_ParseTuple(args, "ii", &touchpad, &finger)) {
        return NULL;
    }
    bool down = false;
    float x = 0, y = 0, pressure = 0;
    if(!SDL_GetGamepadTouchpadFinger(gp, touchpad, finger, &down, &x, &y, &pressure)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return Py_BuildValue("(Nfff)", PyBool_FromLong(down), x, y, pressure);
}

// Apple platforms only; None elsewhere.
static PyObject * PySDL_Gamepad_GetAppleSFSymbolsNameForButton_impl(PySDL_Gamepad *self, PyObject *arg) {
    int button;
    SDL_Gamepad *gp = _gp_int(self, arg, &button);
    return gp ? _str_or_none(SDL_GetGamepadAppleSFSymbolsNameForButton(gp, (SDL_GamepadButton)button)) : NULL;
}

static PyObject * PySDL_Gamepad_GetAppleSFSymbolsNameForAxis_impl(PySDL_Gamepad *self, PyObject *arg) {
    int axis;
    SDL_Gamepad *gp = _gp_int(self, arg, &axis);
    return gp ? _str_or_none(SDL_GetGamepadAppleSFSymbolsNameForAxis(gp, (SDL_GamepadAxis)axis)) : NULL;
}

static PyObject * PySDL_Gamepad_Close_impl(PySDL_Gamepad *self, PyObject *ign) {
    if(NULL != self->gamepad) {
        SDL_CloseGamepad(self->gamepad);
        self->gamepad = NULL;
    }
    Py_RETURN_NONE;
}

//=========================================================
// module functions
//=========================================================

static int _id_arg(PyObject *arg, SDL_JoystickID *out) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return 0;
    }
    *out = (SDL_JoystickID)id;
    return 1;
}

static int _int_arg(PyObject *arg, int *out) {
    long value = PyLong_AsLong(arg);
    if(-1 == value && PyErr_Occurred()) {
        return 0;
    }
    *out = (int)value;
    return 1;
}

// A new, owned wrapper for an already-open gamepad (re-opening by ID bumps
// SDL's refcount), or None when nothing matching is open.
static PyObject * _wrap_open_gamepad(SDL_Gamepad *gp) {
    if(NULL == gp) {
        SDL_ClearError();
        Py_RETURN_NONE;
    }
    return PyObject_CallFunction((PyObject *)&PySDL_Gamepad_Type, "k", (unsigned long)SDL_GetGamepadID(gp));
}

static PyObject * PySDL_GetGamepads_impl(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_JoystickID *ids = SDL_GetGamepads(&count);
    PyObject *list = PyList_New(count);
    for(int idx = 0; list && idx < count; ++idx) {
        PyObject *id = PyLong_FromUnsignedLong(ids[idx]);
        if(NULL == id) {
            Py_CLEAR(list);
            break;
        }
        PyList_SET_ITEM(list, idx, id);
    }
    SDL_free(ids);
    return list;
}

static PyObject * PySDL_IsGameController_impl(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? PyBool_FromLong(SDL_IsGamepad(id)) : NULL;
}

static PyObject * PySDL_GameControllerNameForIndex_impl(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    if(!_id_arg(arg, &id)) {
        return NULL;
    }
    const char *name = SDL_GetGamepadNameForID(id);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_GameControllerPathForIndex_impl(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? _str_or_none(SDL_GetGamepadPathForID(id)) : NULL;
}

static PyObject * PySDL_GameControllerTypeForIndex_impl(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? PyLong_FromLong(SDL_GetGamepadTypeForID(id)) : NULL;
}

static PyObject * PySDL_GameControllerMappingForDeviceIndex_impl(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? _take_mapping(SDL_GetGamepadMappingForID(id)) : NULL;
}

static PyObject * PySDL_GameControllerFromInstanceID_impl(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? _wrap_open_gamepad(SDL_GetGamepadFromID(id)) : NULL;
}

static PyObject * PySDL_GameControllerFromPlayerIndex_impl(PyObject *self, PyObject *arg) {
    int player;
    return _int_arg(arg, &player) ? _wrap_open_gamepad(SDL_GetGamepadFromPlayerIndex(player)) : NULL;
}

static PyObject * PySDL_GameControllerAddMapping_impl(PyObject *self, PyObject *arg) {
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

// Takes a path (str) or the file's contents (bytes-like) -> number of mappings added.
static PyObject * PySDL_GameControllerAddMappingsFromFile_impl(PyObject *self, PyObject *arg) {
    int rc;
    if(PyUnicode_Check(arg)) {
        const char *path = PyUnicode_AsUTF8(arg);
        if(NULL == path) {
            return NULL;
        }
        rc = SDL_AddGamepadMappingsFromFile(path);
    } else {
        Py_buffer buffer;
        if(0 > PyObject_GetBuffer(arg, &buffer, PyBUF_SIMPLE)) {
            return NULL;
        }
        // SDL reads the whole stream before returning, so the buffer can go after.
        SDL_IOStream *io = SDL_IOFromConstMem(buffer.buf, (size_t)buffer.len);
        rc = io ? SDL_AddGamepadMappingsFromIO(io, true) : -1;
        PyBuffer_Release(&buffer);
    }
    if(-1 == rc) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyLong_FromLong(rc);
}

// GetGamepadMappings() -> [str] (every mapping SDL knows)
static PyObject * PySDL_GetGamepadMappings_impl(PyObject *self, PyObject *ign) {
    int count = 0;
    char **mappings = SDL_GetGamepadMappings(&count);
    if(NULL == mappings) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    PyObject *list = PyList_New(count);
    for(int idx = 0; list && idx < count; ++idx) {
        PyObject *text = PyUnicode_FromString(mappings[idx]);
        if(NULL == text) {
            Py_CLEAR(list);
            break;
        }
        PyList_SET_ITEM(list, idx, text);
    }
    SDL_free(mappings);
    return list;
}

static PyObject * PySDL_GameControllerNumMappings_impl(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_free(SDL_GetGamepadMappings(&count));
    return PyLong_FromLong(count);
}

// SDL3 has no per-index lookup: index into SDL_GetGamepadMappings. None if out of range.
static PyObject * PySDL_GameControllerMappingForIndex_impl(PyObject *self, PyObject *arg) {
    int index;
    if(!_int_arg(arg, &index)) {
        return NULL;
    }
    int count = 0;
    char **mappings = SDL_GetGamepadMappings(&count);
    PyObject *result = (mappings && index >= 0 && index < count)
        ? PyUnicode_FromString(mappings[index]) : Py_NewRef(Py_None);
    SDL_free(mappings);
    return result;
}

static PyObject * PySDL_GameControllerMappingForGUID_impl(PyObject *self, PyObject *arg) {
    SDL_GUID guid;
    return PyToGUID(arg, &guid) ? _take_mapping(SDL_GetGamepadMappingForGUID(guid)) : NULL;
}

static PyObject * PySDL_GameControllerGetAxisFromString_impl(PyObject *self, PyObject *arg) {
    const char *name = PyUnicode_AsUTF8(arg);
    return name ? PyLong_FromLong(SDL_GetGamepadAxisFromString(name)) : NULL;
}

static PyObject * PySDL_GameControllerGetButtonFromString_impl(PyObject *self, PyObject *arg) {
    const char *name = PyUnicode_AsUTF8(arg);
    return name ? PyLong_FromLong(SDL_GetGamepadButtonFromString(name)) : NULL;
}

static PyObject * PySDL_GetGamepadTypeFromString_impl(PyObject *self, PyObject *arg) {
    const char *name = PyUnicode_AsUTF8(arg);
    return name ? PyLong_FromLong(SDL_GetGamepadTypeFromString(name)) : NULL;
}

static PyObject * PySDL_GameControllerGetStringForAxis_impl(PyObject *self, PyObject *arg) {
    int axis;
    return _int_arg(arg, &axis) ? _str_or_none(SDL_GetGamepadStringForAxis((SDL_GamepadAxis)axis)) : NULL;
}

static PyObject * PySDL_GameControllerGetStringForButton_impl(PyObject *self, PyObject *arg) {
    int button;
    return _int_arg(arg, &button) ? _str_or_none(SDL_GetGamepadStringForButton((SDL_GamepadButton)button)) : NULL;
}

static PyObject * PySDL_GetGamepadStringForType_impl(PyObject *self, PyObject *arg) {
    int type;
    return _int_arg(arg, &type) ? _str_or_none(SDL_GetGamepadStringForType((SDL_GamepadType)type)) : NULL;
}

static PyObject * PySDL_GameControllerUpdate_impl(PyObject *self, PyObject *ign) {
    SDL_UpdateGamepads();
    Py_RETURN_NONE;
}

static PyObject * PySDL_GameControllerEventState_impl(PyObject *self, PyObject *args) {
    PyObject *state = NULL;
    if(!PyArg_ParseTuple(args, "|O", &state)) {
        return NULL;
    }
    if(state && state != Py_None) {
        SDL_SetGamepadEventsEnabled(PyObject_IsTrue(state) ? true : false);
    }
    return PyBool_FromLong(SDL_GamepadEventsEnabled());
}

// Every module function runs holding SDL's joystick lock, taken without the
// GIL (see PySDL_JoystickLock in pysdl.h).
PYSDL_JOYSTICK_LOCKED(PySDL_GetGamepads,                         PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_IsGameController,                    PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerNameForIndex,          PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerPathForIndex,          PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerTypeForIndex,          PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerMappingForDeviceIndex, PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerFromInstanceID,        PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerFromPlayerIndex,       PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerAddMapping,            PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerAddMappingsFromFile,   PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GetGamepadMappings,                  PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerNumMappings,           PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerMappingForIndex,       PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerMappingForGUID,        PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerGetAxisFromString,     PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerGetButtonFromString,   PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GetGamepadTypeFromString,            PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerGetStringForAxis,      PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerGetStringForButton,    PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GetGamepadStringForType,             PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerUpdate,                PyObject *)
PYSDL_JOYSTICK_LOCKED(PySDL_GameControllerEventState,            PyObject *)

PyMethodDef pysdl_gamepad_methods[] = {
    { "GetGamepads",                      PySDL_GetGamepads,                      METH_NOARGS  },
    { "IsGameController",                 PySDL_IsGameController,                 METH_O       },
    { "IsGamepad",                        PySDL_IsGameController,                 METH_O       },
    { "GameControllerNameForIndex",       PySDL_GameControllerNameForIndex,       METH_O       },
    { "GetGamepadNameForID",              PySDL_GameControllerNameForIndex,       METH_O       },
    { "GameControllerPathForIndex",       PySDL_GameControllerPathForIndex,       METH_O       },
    { "GetGamepadPathForID",              PySDL_GameControllerPathForIndex,       METH_O       },
    { "GameControllerTypeForIndex",       PySDL_GameControllerTypeForIndex,       METH_O       },
    { "GetGamepadTypeForID",              PySDL_GameControllerTypeForIndex,       METH_O       },
    { "GameControllerMappingForDeviceIndex", PySDL_GameControllerMappingForDeviceIndex, METH_O },
    { "GetGamepadMappingForID",           PySDL_GameControllerMappingForDeviceIndex, METH_O    },
    { "GameControllerFromInstanceID",     PySDL_GameControllerFromInstanceID,     METH_O       },
    { "GetGamepadFromID",                 PySDL_GameControllerFromInstanceID,     METH_O       },
    { "GameControllerFromPlayerIndex",    PySDL_GameControllerFromPlayerIndex,    METH_O       },
    { "GetGamepadFromPlayerIndex",        PySDL_GameControllerFromPlayerIndex,    METH_O       },
    { "GameControllerAddMapping",         PySDL_GameControllerAddMapping,         METH_O       },
    { "AddGamepadMapping",                PySDL_GameControllerAddMapping,         METH_O       },
    { "GameControllerAddMappingsFromFile", PySDL_GameControllerAddMappingsFromFile, METH_O     },
    { "AddGamepadMappingsFromFile",       PySDL_GameControllerAddMappingsFromFile, METH_O      },
    { "GetGamepadMappings",               PySDL_GetGamepadMappings,               METH_NOARGS  },
    { "GameControllerNumMappings",        PySDL_GameControllerNumMappings,        METH_NOARGS  },
    { "GameControllerMappingForIndex",    PySDL_GameControllerMappingForIndex,    METH_O       },
    { "GameControllerMappingForGUID",     PySDL_GameControllerMappingForGUID,     METH_O       },
    { "GetGamepadMappingForGUID",         PySDL_GameControllerMappingForGUID,     METH_O       },
    { "GameControllerGetAxisFromString",  PySDL_GameControllerGetAxisFromString,  METH_O       },
    { "GetGamepadAxisFromString",         PySDL_GameControllerGetAxisFromString,  METH_O       },
    { "GameControllerGetButtonFromString", PySDL_GameControllerGetButtonFromString, METH_O     },
    { "GetGamepadButtonFromString",       PySDL_GameControllerGetButtonFromString, METH_O      },
    { "GetGamepadTypeFromString",         PySDL_GetGamepadTypeFromString,         METH_O       },
    { "GameControllerGetStringForAxis",   PySDL_GameControllerGetStringForAxis,   METH_O       },
    { "GetGamepadStringForAxis",          PySDL_GameControllerGetStringForAxis,   METH_O       },
    { "GameControllerGetStringForButton", PySDL_GameControllerGetStringForButton, METH_O       },
    { "GetGamepadStringForButton",        PySDL_GameControllerGetStringForButton, METH_O       },
    { "GetGamepadStringForType",          PySDL_GetGamepadStringForType,          METH_O       },
    { "GameControllerUpdate",             PySDL_GameControllerUpdate,             METH_NOARGS  },
    { "UpdateGamepads",                   PySDL_GameControllerUpdate,             METH_NOARGS  },
    { "GameControllerEventState",         PySDL_GameControllerEventState,         METH_VARARGS },
    { "SetGamepadEventsEnabled",          PySDL_GameControllerEventState,         METH_VARARGS },
    { NULL }
};
