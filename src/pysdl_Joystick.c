#include "pysdl.h"

// SDL3: joysticks are opened by SDL_JoystickID (from SDL_GetJoysticks), not a
// device index. SDL refcounts opens of the same ID, so every Joystick wrapper
// owns its reference - lookups (Gamepad.GetJoystick, JoystickFromInstanceID,
// ...) re-open by ID rather than borrow.

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
static PyObject * PySDL_Joystick_HasRumble         (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_HasRumbleTriggers (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SendEffect        (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_CurrentPowerLevel (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetPowerInfo      (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetConnectionState (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetVendor         (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetProduct        (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetProductVersion (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetFirmwareVersion (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetSerial         (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_Path              (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetType           (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetAxisInitialState (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_GetPlayerIndex    (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetPlayerIndex    (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetVirtualAxis    (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetVirtualButton  (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetVirtualHat     (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetVirtualBall    (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SetVirtualTouchpad (PySDL_Joystick*, PyObject*);
static PyObject * PySDL_Joystick_SendVirtualSensorData (PySDL_Joystick*, PyObject*, PyObject*);
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
    { "HasRumble",         (PyCFunction)PySDL_Joystick_HasRumble,         METH_NOARGS },
    { "HasRumbleTriggers", (PyCFunction)PySDL_Joystick_HasRumbleTriggers, METH_NOARGS },
    { "SendEffect",        (PyCFunction)PySDL_Joystick_SendEffect,        METH_O      },
    { "CurrentPowerLevel", (PyCFunction)PySDL_Joystick_CurrentPowerLevel, METH_NOARGS },
    { "GetPowerInfo",      (PyCFunction)PySDL_Joystick_GetPowerInfo,      METH_NOARGS },
    { "GetConnectionState", (PyCFunction)PySDL_Joystick_GetConnectionState, METH_NOARGS },
    { "GetVendor",         (PyCFunction)PySDL_Joystick_GetVendor,         METH_NOARGS },
    { "GetProduct",        (PyCFunction)PySDL_Joystick_GetProduct,        METH_NOARGS },
    { "GetProductVersion", (PyCFunction)PySDL_Joystick_GetProductVersion, METH_NOARGS },
    { "GetFirmwareVersion", (PyCFunction)PySDL_Joystick_GetFirmwareVersion, METH_NOARGS },
    { "GetSerial",         (PyCFunction)PySDL_Joystick_GetSerial,         METH_NOARGS },
    { "Path",              (PyCFunction)PySDL_Joystick_Path,              METH_NOARGS },
    { "GetType",           (PyCFunction)PySDL_Joystick_GetType,           METH_NOARGS },
    { "GetAxisInitialState", (PyCFunction)PySDL_Joystick_GetAxisInitialState, METH_O },
    { "GetPlayerIndex",    (PyCFunction)PySDL_Joystick_GetPlayerIndex,    METH_NOARGS },
    { "SetPlayerIndex",    (PyCFunction)PySDL_Joystick_SetPlayerIndex,    METH_O      },
    { "SetVirtualAxis",    (PyCFunction)PySDL_Joystick_SetVirtualAxis,    METH_VARARGS },
    { "SetVirtualButton",  (PyCFunction)PySDL_Joystick_SetVirtualButton,  METH_VARARGS },
    { "SetVirtualHat",     (PyCFunction)PySDL_Joystick_SetVirtualHat,     METH_VARARGS },
    { "SetVirtualBall",    (PyCFunction)PySDL_Joystick_SetVirtualBall,    METH_VARARGS },
    { "SetVirtualTouchpad", (PyCFunction)PySDL_Joystick_SetVirtualTouchpad, METH_VARARGS },
    { "SendVirtualSensorData", (PyCFunction)PySDL_Joystick_SendVirtualSensorData, METH_VARARGS | METH_KEYWORDS },
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
    if(NULL != self->joystick) {
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

// Parse a METH_O int argument alongside the open-joystick guard.
static SDL_Joystick * _js_int(PySDL_Joystick *self, PyObject *arg, int *out) {
    SDL_Joystick *js = _js(self);
    if(NULL == js) {
        return NULL;
    }
    long value = PyLong_AsLong(arg);
    if(-1 == value && PyErr_Occurred()) {
        return NULL;
    }
    *out = (int)value;
    return js;
}

// A const char* SDL may return as NULL -> str or None.
static PyObject * _str_or_none(const char *text) {
    if(NULL == text) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(text);
}

static PyObject * _cap(SDL_Joystick *js, const char *name) {
    if(NULL == js) {
        Py_RETURN_FALSE;
    }
    return PyBool_FromLong(SDL_GetBooleanProperty(SDL_GetJoystickProperties(js), name, false));
}

static PyObject * PySDL_Joystick_Name(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    if(NULL == js) return NULL;
    const char *name = SDL_GetJoystickName(js);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_Joystick_GetGUID(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? GUIDToPy(SDL_GetJoystickGUID(js)) : NULL;
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
    int axis;
    SDL_Joystick *js = _js_int(self, arg, &axis);
    return js ? PyLong_FromLong(SDL_GetJoystickAxis(js, axis)) : NULL;
}

static PyObject * PySDL_Joystick_GetButton(PySDL_Joystick *self, PyObject *arg) {
    int button;
    SDL_Joystick *js = _js_int(self, arg, &button);
    return js ? PyBool_FromLong(SDL_GetJoystickButton(js, button)) : NULL;
}

static PyObject * PySDL_Joystick_GetHat(PySDL_Joystick *self, PyObject *arg) {
    int hat;
    SDL_Joystick *js = _js_int(self, arg, &hat);
    return js ? PyLong_FromLong(SDL_GetJoystickHat(js, hat)) : NULL;
}

static PyObject * PySDL_Joystick_GetBall(PySDL_Joystick *self, PyObject *arg) {
    int ball;
    SDL_Joystick *js = _js_int(self, arg, &ball);
    if(NULL == js) return NULL;
    int dx = 0, dy = 0;
    if(!SDL_GetJoystickBall(js, ball, &dx, &dy)) {
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
    return _cap(self->joystick, SDL_PROP_JOYSTICK_CAP_RGB_LED_BOOLEAN);
}

static PyObject * PySDL_Joystick_HasRumble(PySDL_Joystick *self, PyObject *ign) {
    return _cap(self->joystick, SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN);
}

static PyObject * PySDL_Joystick_HasRumbleTriggers(PySDL_Joystick *self, PyObject *ign) {
    return _cap(self->joystick, SDL_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN);
}

// SendEffect(data: bytes-like) -> bool
static PyObject * PySDL_Joystick_SendEffect(PySDL_Joystick *self, PyObject *arg) {
    SDL_Joystick *js = _js(self);
    if(NULL == js) return NULL;
    Py_buffer data;
    if(0 > PyObject_GetBuffer(arg, &data, PyBUF_SIMPLE)) {
        return NULL;
    }
    if(data.len > INT_MAX) {
        PyBuffer_Release(&data);
        PyErr_SetString(PyExc_ValueError, "effect data too large");
        return NULL;
    }
    bool ok = SDL_SendJoystickEffect(js, data.buf, (int)data.len);
    PyBuffer_Release(&data);
    return PyBool_FromLong(ok);
}

// SDL3 dropped power levels: this is the SDL_PowerState (POWERSTATE_*).
static PyObject * PySDL_Joystick_CurrentPowerLevel(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    if(NULL == js) return NULL;
    int percent = -1;
    return PyLong_FromLong(SDL_GetJoystickPowerInfo(js, &percent));
}

// -> (POWERSTATE_*, percent or -1)
static PyObject * PySDL_Joystick_GetPowerInfo(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    if(NULL == js) return NULL;
    int percent = -1;
    SDL_PowerState state = SDL_GetJoystickPowerInfo(js, &percent);
    return Py_BuildValue("(ii)", state, percent);
}

// -> JOYSTICK_CONNECTION_*
static PyObject * PySDL_Joystick_GetConnectionState(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_GetJoystickConnectionState(js)) : NULL;
}

static PyObject * PySDL_Joystick_GetVendor(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_GetJoystickVendor(js)) : NULL;
}

static PyObject * PySDL_Joystick_GetProduct(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_GetJoystickProduct(js)) : NULL;
}

static PyObject * PySDL_Joystick_GetProductVersion(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_GetJoystickProductVersion(js)) : NULL;
}

static PyObject * PySDL_Joystick_GetFirmwareVersion(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_GetJoystickFirmwareVersion(js)) : NULL;
}

static PyObject * PySDL_Joystick_GetSerial(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? _str_or_none(SDL_GetJoystickSerial(js)) : NULL;
}

static PyObject * PySDL_Joystick_Path(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? _str_or_none(SDL_GetJoystickPath(js)) : NULL;
}

static PyObject * PySDL_Joystick_GetType(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_GetJoystickType(js)) : NULL;
}

// -> the axis's initial value, or None if SDL has no initial state for it.
static PyObject * PySDL_Joystick_GetAxisInitialState(PySDL_Joystick *self, PyObject *arg) {
    int axis;
    SDL_Joystick *js = _js_int(self, arg, &axis);
    if(NULL == js) return NULL;
    Sint16 state = 0;
    if(!SDL_GetJoystickAxisInitialState(js, axis, &state)) {
        Py_RETURN_NONE;
    }
    return PyLong_FromLong(state);
}

static PyObject * PySDL_Joystick_GetPlayerIndex(PySDL_Joystick *self, PyObject *ign) {
    SDL_Joystick *js = _js(self);
    return js ? PyLong_FromLong(SDL_GetJoystickPlayerIndex(js)) : NULL;
}

// SetPlayerIndex(index) - -1 clears it.
static PyObject * PySDL_Joystick_SetPlayerIndex(PySDL_Joystick *self, PyObject *arg) {
    int player;
    SDL_Joystick *js = _js_int(self, arg, &player);
    if(NULL == js) return NULL;
    if(!SDL_SetJoystickPlayerIndex(js, player)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
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

// SetVirtualBall(ball, xrel, yrel) - accumulates until the next update.
static PyObject * PySDL_Joystick_SetVirtualBall(PySDL_Joystick *self, PyObject *args) {
    int ball;
    short xrel, yrel;
    SDL_Joystick *js = _js(self);
    if(NULL == js || !PyArg_ParseTuple(args, "ihh", &ball, &xrel, &yrel)) {
        return NULL;
    }
    if(!SDL_SetJoystickVirtualBall(js, ball, xrel, yrel)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

// SetVirtualTouchpad(touchpad, finger, down, x, y, pressure) - x/y/pressure 0..1
static PyObject * PySDL_Joystick_SetVirtualTouchpad(PySDL_Joystick *self, PyObject *args) {
    int touchpad, finger, down;
    float x, y, pressure;
    SDL_Joystick *js = _js(self);
    if(NULL == js || !PyArg_ParseTuple(args, "iipfff", &touchpad, &finger, &down, &x, &y, &pressure)) {
        return NULL;
    }
    if(!SDL_SetJoystickVirtualTouchpad(js, touchpad, finger, down ? true : false, x, y, pressure)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

// SendVirtualSensorData(type, values, sensor_timestamp=0) - values: 1..16 floats,
// delivered (if the sensor is enabled) on the next update.
static PyObject * PySDL_Joystick_SendVirtualSensorData(PySDL_Joystick *self, PyObject *args, PyObject *kwds) {
    int type;
    PyObject *values;
    unsigned long long timestamp = 0;
    SDL_Joystick *js = _js(self);
    static char *kwlist[] = {"type", "values", "sensor_timestamp", NULL};
    if(NULL == js || !PyArg_ParseTupleAndKeywords(args, kwds, "iO|K", kwlist, &type, &values, &timestamp)) {
        return NULL;
    }
    // Before 3.2.22 SDL's sensor queue never grew past one slot, so a second
    // sample before the next update wrote past the end of the heap buffer.
    if(SDL_GetVersion() < SDL_VERSIONNUM(3, 2, 22)) {
        PyErr_SetString(pysdl_Error, "SendVirtualSensorData needs SDL >= 3.2.22");
        return NULL;
    }

    PyObject *fast = PySequence_Fast(values, "values must be a sequence of floats");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(fast);
    if(count < 1 || count > 16) {
        Py_DECREF(fast);
        PyErr_SetString(PyExc_ValueError, "values takes 1..16 floats");
        return NULL;
    }
    float data[16];
    for(Py_ssize_t idx = 0; idx < count; ++idx) {
        double value = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(fast, idx));
        if(-1.0 == value && PyErr_Occurred()) {
            Py_DECREF(fast);
            return NULL;
        }
        data[idx] = (float)value;
    }
    Py_DECREF(fast);

    if(!SDL_SendJoystickVirtualSensorData(js, (SDL_SensorType)type, timestamp, data, (int)count)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Joystick_Close(PySDL_Joystick *self, PyObject *ign) {
    if(NULL != self->joystick) {
        SDL_CloseJoystick(self->joystick);
    }
    self->joystick = NULL;
    Py_RETURN_NONE;
}

//=========================================================
// GUID helpers (shared with pysdl_Gamepad.c)
//=========================================================

PyObject * GUIDToPy(SDL_GUID guid) {
    char buffer[33];
    SDL_GUIDToString(guid, buffer, sizeof(buffer));
    return PyUnicode_FromString(buffer);
}

int PyToGUID(PyObject *obj, SDL_GUID *guid) {
    if(PyUnicode_Check(obj)) {
        Py_ssize_t len;
        const char *text = PyUnicode_AsUTF8AndSize(obj, &len);
        if(NULL == text) {
            return 0;
        }
        int hex = (32 == len);
        for(Py_ssize_t idx = 0; hex && idx < len; ++idx) {
            hex = SDL_isxdigit((unsigned char)text[idx]);
        }
        if(!hex) {
            PyErr_SetString(PyExc_ValueError, "GUID string must be 32 hex digits");
            return 0;
        }
        *guid = SDL_StringToGUID(text);
        return 1;
    }

    Py_buffer raw;
    if(0 > PyObject_GetBuffer(obj, &raw, PyBUF_SIMPLE)) {
        PyErr_SetString(PyExc_TypeError, "GUID must be a 32-digit hex str or 16 bytes");
        return 0;
    }
    if((Py_ssize_t)sizeof(guid->data) != raw.len) {
        PyBuffer_Release(&raw);
        PyErr_SetString(PyExc_ValueError, "GUID bytes must be 16 long");
        return 0;
    }
    SDL_memcpy(guid->data, raw.buf, sizeof(guid->data));
    PyBuffer_Release(&raw);
    return 1;
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

// A new, owned wrapper for an already-open joystick (re-opening by ID bumps
// SDL's refcount, so closing one wrapper never invalidates another), or None.
static PyObject * _wrap_open_joystick(SDL_Joystick *js) {
    if(NULL == js) {
        SDL_ClearError();
        Py_RETURN_NONE;
    }
    return PyObject_CallFunction((PyObject *)&PySDL_Joystick_Type, "k", (unsigned long)SDL_GetJoystickID(js));
}

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
        PyObject *id = PyLong_FromUnsignedLong(ids[idx]);
        if(NULL == id) {
            Py_DECREF(list);
            SDL_free(ids);
            return NULL;
        }
        PyList_SET_ITEM(list, idx, id);
    }
    SDL_free(ids);
    return list;
}

static PyObject * PySDL_JoystickNameForIndex(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    if(!_id_arg(arg, &id)) {
        return NULL;
    }
    const char *name = SDL_GetJoystickNameForID(id);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_JoystickGetDeviceGUID(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? GUIDToPy(SDL_GetJoystickGUIDForID(id)) : NULL;
}

static PyObject * PySDL_JoystickGetDeviceVendor(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? PyLong_FromLong(SDL_GetJoystickVendorForID(id)) : NULL;
}

static PyObject * PySDL_JoystickGetDeviceProduct(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? PyLong_FromLong(SDL_GetJoystickProductForID(id)) : NULL;
}

static PyObject * PySDL_JoystickGetDeviceProductVersion(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? PyLong_FromLong(SDL_GetJoystickProductVersionForID(id)) : NULL;
}

static PyObject * PySDL_JoystickGetDeviceType(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? PyLong_FromLong(SDL_GetJoystickTypeForID(id)) : NULL;
}

static PyObject * PySDL_JoystickGetDevicePlayerIndex(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? PyLong_FromLong(SDL_GetJoystickPlayerIndexForID(id)) : NULL;
}

static PyObject * PySDL_JoystickPathForIndex(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? _str_or_none(SDL_GetJoystickPathForID(id)) : NULL;
}

static PyObject * PySDL_JoystickFromInstanceID(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? _wrap_open_joystick(SDL_GetJoystickFromID(id)) : NULL;
}

static PyObject * PySDL_JoystickFromPlayerIndex(PyObject *self, PyObject *arg) {
    int player;
    return _int_arg(arg, &player) ? _wrap_open_joystick(SDL_GetJoystickFromPlayerIndex(player)) : NULL;
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

// Virtual-joystick callbacks run with this lock held: don't hold it across a
// call that waits on another thread which may be running one.
static PyObject * PySDL_LockJoysticks(PyObject *self, PyObject *ign) {
    SDL_LockJoysticks();
    Py_RETURN_NONE;
}

static PyObject * PySDL_UnlockJoysticks(PyObject *self, PyObject *ign) {
    SDL_UnlockJoysticks();
    Py_RETURN_NONE;
}

// GUIDs are 32-char hex strings everywhere else in the binding; these convert
// to and from the raw 16 bytes. Both accept either form.
static PyObject * PySDL_GUIDFromString(PyObject *self, PyObject *arg) {
    SDL_GUID guid;
    if(!PyToGUID(arg, &guid)) {
        return NULL;
    }
    return PyBytes_FromStringAndSize((const char *)guid.data, sizeof(guid.data));
}

static PyObject * PySDL_GUIDToString(PyObject *self, PyObject *arg) {
    SDL_GUID guid;
    return PyToGUID(arg, &guid) ? GUIDToPy(guid) : NULL;
}

// -> (vendor, product, version, crc16)
static PyObject * PySDL_GetJoystickGUIDInfo(PyObject *self, PyObject *arg) {
    SDL_GUID guid;
    if(!PyToGUID(arg, &guid)) {
        return NULL;
    }
    Uint16 vendor = 0, product = 0, version = 0, crc16 = 0;
    SDL_GetJoystickGUIDInfo(guid, &vendor, &product, &version, &crc16);
    return Py_BuildValue("(iiii)", vendor, product, version, crc16);
}

//=========================================================
// virtual joysticks
//=========================================================

// Callback slots, in the keyword order of JoystickAttachVirtual.
enum { VJ_UPDATE, VJ_PLAYER, VJ_RUMBLE, VJ_TRIGGERS, VJ_LED, VJ_EFFECT, VJ_SENSORS, VJ_COUNT };

// SDL is never handed a Python object: a virtual joystick with callbacks gets
// an integer token as its userdata, and each callback looks the token up -
// under the GIL - in _vjoys (token -> tuple of VJ_COUNT callables-or-None).
// The entry is made before the attach (SDL may call SetPlayerIndex from inside
// it) and dropped by SDL's Cleanup callback, which runs whenever the device
// goes away: JoystickDetachVirtual, QuitSubSystem / Quit, or a failed attach.
// SDL calls these with its joystick lock held.
static PyObject *_vjoys;
static unsigned long long _vj_next_token;

// Call slot `slot` of the joystick `token` with Py_BuildValue(fmt, ...) args.
// Returns 1 if it returned None or a true value, 0 on a false value or an
// exception (printed). May run from a dealloc (SDL_CloseJoystick stops a
// rumble), so any exception already pending is set aside meanwhile.
static int _vj_call(void *token, int slot, const char *fmt, ...) {
    PyGILState_STATE gil;
    if(!PySDL_ThreadEnter(&gil)) {
        return 0;
    }
    PyObject *etype, *evalue, *etb;
    PyErr_Fetch(&etype, &evalue, &etb);

    int ok = 0;
    PyObject *key = PyLong_FromUnsignedLongLong((unsigned long long)(uintptr_t)token);
    PyObject *entry = (key && _vjoys) ? PyDict_GetItemWithError(_vjoys, key) : NULL;
    Py_XDECREF(key);
    PyObject *fn = entry ? PyTuple_GET_ITEM(entry, slot) : NULL;
    if(NULL != fn && Py_None != fn) {
        Py_INCREF(fn);  // the callback may detach the device, dropping the entry
        va_list va;
        va_start(va, fmt);
        PyObject *args = Py_VaBuildValue(fmt, va);
        va_end(va);
        PyObject *result = args ? PyObject_CallObject(fn, args) : NULL;
        Py_XDECREF(args);
        Py_DECREF(fn);
        if(NULL != result) {
            ok = (Py_None == result) ? 1 : (1 == PyObject_IsTrue(result));
            Py_DECREF(result);
        }
    }
    if(PyErr_Occurred()) {
        PyErr_Print();
    }

    PyErr_Restore(etype, evalue, etb);
    PySDL_ThreadLeave(gil);
    return ok;
}

static bool _vj_result(int ok, const char *what) {
    return ok ? true : SDL_SetError("virtual joystick %s callback failed", what);
}

static void SDLCALL _vj_update(void *token) {
    _vj_call(token, VJ_UPDATE, "()");
}

static void SDLCALL _vj_player(void *token, int player) {
    _vj_call(token, VJ_PLAYER, "(i)", player);
}

static bool SDLCALL _vj_rumble(void *token, Uint16 low, Uint16 high) {
    return _vj_result(_vj_call(token, VJ_RUMBLE, "(ii)", (int)low, (int)high), "rumble");
}

static bool SDLCALL _vj_triggers(void *token, Uint16 left, Uint16 right) {
    return _vj_result(_vj_call(token, VJ_TRIGGERS, "(ii)", (int)left, (int)right), "rumble_triggers");
}

static bool SDLCALL _vj_led(void *token, Uint8 r, Uint8 g, Uint8 b) {
    return _vj_result(_vj_call(token, VJ_LED, "(iii)", (int)r, (int)g, (int)b), "set_led");
}

static bool SDLCALL _vj_effect(void *token, const void *data, int size) {
    if(NULL == data || size < 0) {
        data = "";
        size = 0;
    }
    return _vj_result(_vj_call(token, VJ_EFFECT, "(y#)", (const char *)data, (Py_ssize_t)size), "send_effect");
}

static bool SDLCALL _vj_sensors(void *token, bool enabled) {
    return _vj_result(_vj_call(token, VJ_SENSORS, "(N)", PyBool_FromLong(enabled)), "set_sensors_enabled");
}

static void SDLCALL _vj_cleanup(void *token) {
    PyGILState_STATE gil;
    if(!PySDL_ThreadEnter(&gil)) {
        return;  // interpreter gone: the entry goes with it
    }
    PyObject *etype, *evalue, *etb;
    PyErr_Fetch(&etype, &evalue, &etb);

    PyObject *key = PyLong_FromUnsignedLongLong((unsigned long long)(uintptr_t)token);
    if(NULL == key || NULL == _vjoys || 0 > PyDict_DelItem(_vjoys, key)) {
        PyErr_Clear();  // already gone
    }
    Py_XDECREF(key);

    PyErr_Restore(etype, evalue, etb);
    PySDL_ThreadLeave(gil);
}

// Each item of `touchpads` is a finger count.
static SDL_VirtualJoystickTouchpadDesc * _touchpads(PyObject *src, Uint16 *count) {
    PyObject *fast = PySequence_Fast(src, "touchpads must be a sequence of finger counts");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    if(n < 1 || n > 0xFFFF) {
        Py_DECREF(fast);
        PyErr_SetString(PyExc_ValueError, "touchpads takes 1..65535 items");
        return NULL;
    }
    SDL_VirtualJoystickTouchpadDesc *descs = PyMem_Calloc((size_t)n, sizeof(*descs));
    if(NULL == descs) {
        Py_DECREF(fast);
        PyErr_NoMemory();
        return NULL;
    }
    for(Py_ssize_t idx = 0; idx < n; ++idx) {
        long fingers = PyLong_AsLong(PySequence_Fast_GET_ITEM(fast, idx));
        if(fingers < 1 || fingers > 0xFFFF) {
            if(!PyErr_Occurred()) {
                PyErr_SetString(PyExc_ValueError, "a touchpad takes 1..65535 fingers");
            }
            PyMem_Free(descs);
            Py_DECREF(fast);
            return NULL;
        }
        descs[idx].nfingers = (Uint16)fingers;
    }
    Py_DECREF(fast);
    *count = (Uint16)n;
    return descs;
}

// Each item of `sensors` is a SENSOR_* type or a (type, rate_hz) pair.
static SDL_VirtualJoystickSensorDesc * _sensors(PyObject *src, Uint16 *count) {
    PyObject *fast = PySequence_Fast(src, "sensors must be a sequence of types or (type, rate) pairs");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    if(n < 1 || n > 0xFFFF) {
        Py_DECREF(fast);
        PyErr_SetString(PyExc_ValueError, "sensors takes 1..65535 items");
        return NULL;
    }
    SDL_VirtualJoystickSensorDesc *descs = PyMem_Calloc((size_t)n, sizeof(*descs));
    if(NULL == descs) {
        Py_DECREF(fast);
        PyErr_NoMemory();
        return NULL;
    }
    for(Py_ssize_t idx = 0; idx < n; ++idx) {
        PyObject *item = PySequence_Fast_GET_ITEM(fast, idx);
        long type;
        double rate = 0.0;
        if(PyLong_Check(item)) {
            type = PyLong_AsLong(item);
        } else {
            PyObject *pair = PySequence_Fast(item, "a sensor is a type or a (type, rate) pair");
            if(NULL != pair && 2 != PySequence_Fast_GET_SIZE(pair)) {
                PyErr_SetString(PyExc_ValueError, "a sensor is a type or a (type, rate) pair");
                Py_CLEAR(pair);
            }
            type = pair ? PyLong_AsLong(PySequence_Fast_GET_ITEM(pair, 0)) : -1;
            if(pair && !PyErr_Occurred()) {
                rate = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(pair, 1));
            }
            Py_XDECREF(pair);
        }
        if(PyErr_Occurred()) {
            PyMem_Free(descs);
            Py_DECREF(fast);
            return NULL;
        }
        descs[idx].type = (SDL_SensorType)type;
        descs[idx].rate = (float)rate;
    }
    Py_DECREF(fast);
    *count = (Uint16)n;
    return descs;
}

// JoystickAttachVirtual(type=JOYSTICK_TYPE_GAMEPAD, naxes=0, nbuttons=0, nhats=0, *,
//     nballs=0, vendor_id=0, product_id=0, button_mask=0, axis_mask=0, name=None,
//     touchpads=None, sensors=None, update=None, set_player_index=None, rumble=None,
//     rumble_triggers=None, set_led=None, send_effect=None, set_sensors_enabled=None)
// -> instance id. Only the callbacks given are wired, so e.g. HasRumble()
// reflects whether rumble= was passed. update() and set_player_index(i) return
// nothing; the rest report success by returning None or a true value.
static PyObject * PySDL_JoystickAttachVirtual(PyObject *self, PyObject *args, PyObject *kwds) {
    int type = SDL_JOYSTICK_TYPE_GAMEPAD;
    int naxes = 0, nbuttons = 0, nhats = 0, nballs = 0;
    unsigned short vendor_id = 0, product_id = 0;
    unsigned int button_mask = 0, axis_mask = 0;
    const char *name = NULL;
    PyObject *touchpads = Py_None, *sensors = Py_None;
    PyObject *fns[VJ_COUNT] = { Py_None, Py_None, Py_None, Py_None, Py_None, Py_None, Py_None };

    static char *kwlist[] = {
        "type", "naxes", "nbuttons", "nhats",
        "nballs", "vendor_id", "product_id", "button_mask", "axis_mask", "name",
        "touchpads", "sensors",
        "update", "set_player_index", "rumble", "rumble_triggers", "set_led",
        "send_effect", "set_sensors_enabled",
        NULL
    };
    const int first_fn = 12;
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|iiii$iHHIIzOOOOOOOOO", kwlist,
        &type, &naxes, &nbuttons, &nhats,
        &nballs, &vendor_id, &product_id, &button_mask, &axis_mask, &name,
        &touchpads, &sensors,
        &fns[VJ_UPDATE], &fns[VJ_PLAYER], &fns[VJ_RUMBLE], &fns[VJ_TRIGGERS],
        &fns[VJ_LED], &fns[VJ_EFFECT], &fns[VJ_SENSORS])) {
        return NULL;
    }
    // SDL walks axes / buttons / hats / balls with a Uint8 index.
    if(naxes < 0 || naxes > 255 || nbuttons < 0 || nbuttons > 255
        || nhats < 0 || nhats > 255 || nballs < 0 || nballs > 255) {
        PyErr_SetString(PyExc_ValueError, "naxes, nbuttons, nhats and nballs must be 0..255");
        return NULL;
    }

    int any = 0;
    for(int idx = 0; idx < VJ_COUNT; ++idx) {
        if(fns[idx] != Py_None) {
            if(!PyCallable_Check(fns[idx])) {
                PyErr_Format(PyExc_TypeError, "%s must be callable or None", kwlist[first_fn + idx]);
                return NULL;
            }
            any = 1;
        }
    }

    SDL_VirtualJoystickDesc desc;
    SDL_INIT_INTERFACE(&desc);
    desc.type        = (Uint16)type;
    desc.naxes       = (Uint16)naxes;
    desc.nbuttons    = (Uint16)nbuttons;
    desc.nhats       = (Uint16)nhats;
    desc.nballs      = (Uint16)nballs;
    desc.vendor_id   = vendor_id;
    desc.product_id  = product_id;
    desc.button_mask = button_mask;
    desc.axis_mask   = axis_mask;
    desc.name        = name;  // SDL copies the name and both arrays

    SDL_VirtualJoystickTouchpadDesc *pads = NULL;
    SDL_VirtualJoystickSensorDesc *sens = NULL;
    PyObject *key = NULL;
    PyObject *result = NULL;

    if(touchpads != Py_None && NULL == (pads = _touchpads(touchpads, &desc.ntouchpads))) {
        goto done;
    }
    if(sensors != Py_None && NULL == (sens = _sensors(sensors, &desc.nsensors))) {
        goto done;
    }
    desc.touchpads = pads;
    desc.sensors = sens;

    if(any) {
        if(NULL == _vjoys && NULL == (_vjoys = PyDict_New())) {
            goto done;
        }
        PyObject *entry = PyTuple_Pack(VJ_COUNT, fns[0], fns[1], fns[2], fns[3], fns[4], fns[5], fns[6]);
        unsigned long long token = ++_vj_next_token;
        key = PyLong_FromUnsignedLongLong(token);
        if(NULL == entry || NULL == key || 0 > PyDict_SetItem(_vjoys, key, entry)) {
            Py_XDECREF(entry);
            goto done;
        }
        Py_DECREF(entry);  // the dict holds it now

        desc.userdata = (void *)(uintptr_t)token;
        desc.Cleanup  = _vj_cleanup;
        if(fns[VJ_UPDATE]   != Py_None) desc.Update            = _vj_update;
        if(fns[VJ_PLAYER]   != Py_None) desc.SetPlayerIndex    = _vj_player;
        if(fns[VJ_RUMBLE]   != Py_None) desc.Rumble            = _vj_rumble;
        if(fns[VJ_TRIGGERS] != Py_None) desc.RumbleTriggers    = _vj_triggers;
        if(fns[VJ_LED]      != Py_None) desc.SetLED            = _vj_led;
        if(fns[VJ_EFFECT]   != Py_None) desc.SendEffect        = _vj_effect;
        if(fns[VJ_SENSORS]  != Py_None) desc.SetSensorsEnabled = _vj_sensors;
    }

    SDL_JoystickID id = SDL_AttachVirtualJoystick(&desc);
    if(0 == id) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        if(NULL != key) {
            // SDL runs Cleanup only once it has copied the descriptor.
            PyObject *etype, *evalue, *etb;
            PyErr_Fetch(&etype, &evalue, &etb);
            if(0 > PyDict_DelItem(_vjoys, key)) {
                PyErr_Clear();
            }
            PyErr_Restore(etype, evalue, etb);
        }
        goto done;
    }
    result = PyLong_FromUnsignedLong(id);

done:
    Py_XDECREF(key);
    PyMem_Free(pads);
    PyMem_Free(sens);
    return result;
}

// Detaching runs SDL's Cleanup, which releases the device's callbacks.
static PyObject * PySDL_JoystickDetachVirtual(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    if(!_id_arg(arg, &id)) {
        return NULL;
    }
    if(!SDL_DetachVirtualJoystick(id)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_JoystickIsVirtual(PyObject *self, PyObject *arg) {
    SDL_JoystickID id;
    return _id_arg(arg, &id) ? PyBool_FromLong(SDL_IsJoystickVirtual(id)) : NULL;
}

PyMethodDef pysdl_joystick_methods[] = {
    { "NumJoysticks",           PySDL_NumJoysticks,           METH_NOARGS  },
    { "GetJoysticks",           PySDL_GetJoysticks,           METH_NOARGS  },
    { "JoystickNameForIndex",   PySDL_JoystickNameForIndex,   METH_O       },
    { "GetJoystickNameForID",   PySDL_JoystickNameForIndex,   METH_O       },
    { "JoystickGetDeviceGUID",           PySDL_JoystickGetDeviceGUID,           METH_O },
    { "GetJoystickGUIDForID",            PySDL_JoystickGetDeviceGUID,           METH_O },
    { "JoystickGetDeviceVendor",         PySDL_JoystickGetDeviceVendor,         METH_O },
    { "GetJoystickVendorForID",          PySDL_JoystickGetDeviceVendor,         METH_O },
    { "JoystickGetDeviceProduct",        PySDL_JoystickGetDeviceProduct,        METH_O },
    { "GetJoystickProductForID",         PySDL_JoystickGetDeviceProduct,        METH_O },
    { "JoystickGetDeviceProductVersion", PySDL_JoystickGetDeviceProductVersion, METH_O },
    { "GetJoystickProductVersionForID",  PySDL_JoystickGetDeviceProductVersion, METH_O },
    { "JoystickGetDeviceType",           PySDL_JoystickGetDeviceType,           METH_O },
    { "GetJoystickTypeForID",            PySDL_JoystickGetDeviceType,           METH_O },
    { "JoystickGetDevicePlayerIndex",    PySDL_JoystickGetDevicePlayerIndex,    METH_O },
    { "GetJoystickPlayerIndexForID",     PySDL_JoystickGetDevicePlayerIndex,    METH_O },
    { "JoystickPathForIndex",            PySDL_JoystickPathForIndex,            METH_O },
    { "GetJoystickPathForID",            PySDL_JoystickPathForIndex,            METH_O },
    { "JoystickFromInstanceID",          PySDL_JoystickFromInstanceID,          METH_O },
    { "GetJoystickFromID",               PySDL_JoystickFromInstanceID,          METH_O },
    { "JoystickFromPlayerIndex",         PySDL_JoystickFromPlayerIndex,         METH_O },
    { "GetJoystickFromPlayerIndex",      PySDL_JoystickFromPlayerIndex,         METH_O },
    { "JoystickUpdate",         PySDL_JoystickUpdate,         METH_NOARGS  },
    { "UpdateJoysticks",        PySDL_JoystickUpdate,         METH_NOARGS  },
    { "JoystickEventState",     PySDL_JoystickEventState,     METH_VARARGS },
    { "LockJoysticks",          PySDL_LockJoysticks,          METH_NOARGS  },
    { "UnlockJoysticks",        PySDL_UnlockJoysticks,        METH_NOARGS  },
    { "JoystickGetGUIDFromString", PySDL_GUIDFromString,      METH_O       },
    { "GUIDFromString",         PySDL_GUIDFromString,         METH_O       },
    { "StringToGUID",           PySDL_GUIDFromString,         METH_O       },
    { "JoystickGetGUIDString",  PySDL_GUIDToString,           METH_O       },
    { "GUIDToString",           PySDL_GUIDToString,           METH_O       },
    { "GetJoystickGUIDInfo",    PySDL_GetJoystickGUIDInfo,    METH_O       },
    { "JoystickAttachVirtual",  (PyCFunction)PySDL_JoystickAttachVirtual, METH_VARARGS | METH_KEYWORDS },
    { "JoystickAttachVirtualEx", (PyCFunction)PySDL_JoystickAttachVirtual, METH_VARARGS | METH_KEYWORDS },
    { "AttachVirtualJoystick",  (PyCFunction)PySDL_JoystickAttachVirtual, METH_VARARGS | METH_KEYWORDS },
    { "JoystickDetachVirtual",  PySDL_JoystickDetachVirtual,  METH_O       },
    { "DetachVirtualJoystick",  PySDL_JoystickDetachVirtual,  METH_O       },
    { "JoystickIsVirtual",      PySDL_JoystickIsVirtual,      METH_O       },
    { "IsJoystickVirtual",      PySDL_JoystickIsVirtual,      METH_O       },
    { NULL }
};
