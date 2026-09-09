#include "pysdl.h"

static int        PySDL_GameController_Type_init    (PySDL_GameController*, PyObject*, PyObject*);
static void       PySDL_GameController_Type_dealloc (PySDL_GameController*);

static PyObject * PySDL_GameController_Name           (PySDL_GameController*, PyObject*);
static PyObject * PySDL_GameController_Attached       (PySDL_GameController*, PyObject*);
static PyObject * PySDL_GameController_GetAxis        (PySDL_GameController*, PyObject*);
static PyObject * PySDL_GameController_GetButton      (PySDL_GameController*, PyObject*);
static PyObject * PySDL_GameController_Mapping        (PySDL_GameController*, PyObject*);
static PyObject * PySDL_GameController_GetJoystick    (PySDL_GameController*, PyObject*);
static PyObject * PySDL_GameController_Rumble         (PySDL_GameController*, PyObject*);
#if SDL_VERSION_ATLEAST(2,0,14)
static PyObject * PySDL_GameController_RumbleTriggers (PySDL_GameController*, PyObject*);
static PyObject * PySDL_GameController_SetLED         (PySDL_GameController*, PyObject*);
#endif
#if SDL_VERSION_ATLEAST(2,0,12)
static PyObject * PySDL_GameController_GetType        (PySDL_GameController*, PyObject*);
#endif
static PyObject * PySDL_GameController_Close          (PySDL_GameController*, PyObject*);

static PyMethodDef PySDL_GameController_methods[] = {
    { "Name",           (PyCFunction)PySDL_GameController_Name,           METH_NOARGS },
    { "Attached",       (PyCFunction)PySDL_GameController_Attached,       METH_NOARGS },
    { "GetAxis",        (PyCFunction)PySDL_GameController_GetAxis,        METH_O      },
    { "GetButton",      (PyCFunction)PySDL_GameController_GetButton,      METH_O      },
    { "Mapping",        (PyCFunction)PySDL_GameController_Mapping,        METH_NOARGS },
    { "GetJoystick",    (PyCFunction)PySDL_GameController_GetJoystick,    METH_NOARGS },
    { "Rumble",         (PyCFunction)PySDL_GameController_Rumble,         METH_VARARGS },
#if SDL_VERSION_ATLEAST(2,0,14)
    { "RumbleTriggers", (PyCFunction)PySDL_GameController_RumbleTriggers, METH_VARARGS },
    { "SetLED",         (PyCFunction)PySDL_GameController_SetLED,         METH_VARARGS },
#endif
#if SDL_VERSION_ATLEAST(2,0,12)
    { "GetType",        (PyCFunction)PySDL_GameController_GetType,        METH_NOARGS },
#endif
    { "Close",          (PyCFunction)PySDL_GameController_Close,          METH_NOARGS },
    { NULL }
};

PyTypeObject PySDL_GameController_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.GameController",
    .tp_basicsize = sizeof(PySDL_GameController),
    .tp_dealloc   = (destructor)PySDL_GameController_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.GameController(device_index)",
    .tp_methods   = PySDL_GameController_methods,
    .tp_init      = (initproc)PySDL_GameController_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_GameController_Type_init(PySDL_GameController *self, PyObject *args, PyObject *kwds) {
    int index = -1;
    if(!PyArg_ParseTuple(args, "|i", &index)) {
        return -1;
    }

    self->controller = NULL;

    if(index >= 0) {
        self->controller = SDL_GameControllerOpen(index);
        if(NULL == self->controller) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_GameController_Type_dealloc(PySDL_GameController *self) {
    if(NULL != self->controller) {
        SDL_GameControllerClose(self->controller);
        self->controller = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static SDL_GameController * _gc(PySDL_GameController *self) {
    if(NULL == self->controller) {
        PyErr_SetString(pysdl_Error, "GameController is not open");
        return NULL;
    }
    return self->controller;
}

static PyObject * PySDL_GameController_Name(PySDL_GameController *self, PyObject *ign) {
    SDL_GameController *gc = _gc(self);
    if(NULL == gc) return NULL;
    const char *name = SDL_GameControllerName(gc);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_GameController_Attached(PySDL_GameController *self, PyObject *ign) {
    return PyBool_FromLong(self->controller && SDL_GameControllerGetAttached(self->controller));
}

static PyObject * PySDL_GameController_GetAxis(PySDL_GameController *self, PyObject *arg) {
    SDL_GameController *gc = _gc(self);
    long axis = PyLong_AsLong(arg);
    if(NULL == gc || (-1 == axis && PyErr_Occurred())) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)axis));
}

static PyObject * PySDL_GameController_GetButton(PySDL_GameController *self, PyObject *arg) {
    SDL_GameController *gc = _gc(self);
    long button = PyLong_AsLong(arg);
    if(NULL == gc || (-1 == button && PyErr_Occurred())) {
        return NULL;
    }
    return PyBool_FromLong(SDL_GameControllerGetButton(gc, (SDL_GameControllerButton)button));
}

static PyObject * PySDL_GameController_Mapping(PySDL_GameController *self, PyObject *ign) {
    SDL_GameController *gc = _gc(self);
    if(NULL == gc) return NULL;
    char *mapping = SDL_GameControllerMapping(gc);
    if(NULL == mapping) {
        Py_RETURN_NONE;
    }
    PyObject *result = PyUnicode_FromString(mapping);
    SDL_free(mapping);
    return result;
}

static PyObject * PySDL_GameController_GetJoystick(PySDL_GameController *self, PyObject *ign) {
    SDL_GameController *gc = _gc(self);
    if(NULL == gc) return NULL;

    SDL_Joystick *joystick = SDL_GameControllerGetJoystick(gc);
    if(NULL == joystick) {
        Py_RETURN_NONE;
    }
    PySDL_Joystick *wrapper = (PySDL_Joystick *)PySDL_New(&PySDL_Joystick_Type);
    if(NULL == wrapper) {
        return NULL;
    }
    wrapper->joystick = joystick;  // owned by the controller
    wrapper->shouldFree = 0;
    return (PyObject *)wrapper;
}

static PyObject * PySDL_GameController_Rumble(PySDL_GameController *self, PyObject *args) {
    unsigned int low, high, ms;
    SDL_GameController *gc = _gc(self);
    if(NULL == gc || !PyArg_ParseTuple(args, "III", &low, &high, &ms)) {
        return NULL;
    }
    return PyBool_FromLong(0 == SDL_GameControllerRumble(gc, (Uint16)low, (Uint16)high, ms));
}

#if SDL_VERSION_ATLEAST(2,0,14)
static PyObject * PySDL_GameController_RumbleTriggers(PySDL_GameController *self, PyObject *args) {
    unsigned int left, right, ms;
    SDL_GameController *gc = _gc(self);
    if(NULL == gc || !PyArg_ParseTuple(args, "III", &left, &right, &ms)) {
        return NULL;
    }
    return PyBool_FromLong(0 == SDL_GameControllerRumbleTriggers(gc, (Uint16)left, (Uint16)right, ms));
}

static PyObject * PySDL_GameController_SetLED(PySDL_GameController *self, PyObject *args) {
    unsigned char r, g, b;
    SDL_GameController *gc = _gc(self);
    if(NULL == gc || !PyArg_ParseTuple(args, "bbb", &r, &g, &b)) {
        return NULL;
    }
    return PyBool_FromLong(0 == SDL_GameControllerSetLED(gc, r, g, b));
}
#endif

#if SDL_VERSION_ATLEAST(2,0,12)
static PyObject * PySDL_GameController_GetType(PySDL_GameController *self, PyObject *ign) {
    SDL_GameController *gc = _gc(self);
    return gc ? PyLong_FromLong(SDL_GameControllerGetType(gc)) : NULL;
}
#endif

static PyObject * PySDL_GameController_Close(PySDL_GameController *self, PyObject *ign) {
    if(NULL != self->controller) {
        SDL_GameControllerClose(self->controller);
        self->controller = NULL;
    }
    Py_RETURN_NONE;
}

//=========================================================
// module functions
//=========================================================

static PyObject * PySDL_IsGameController(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    return PyBool_FromLong(SDL_IsGameController((int)index));
}

static PyObject * PySDL_GameControllerNameForIndex(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_GameControllerNameForIndex((int)index);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_GameControllerAddMapping(PyObject *self, PyObject *arg) {
    const char *mapping = PyUnicode_AsUTF8(arg);
    if(NULL == mapping) {
        return NULL;
    }
    int rc = SDL_GameControllerAddMapping(mapping);
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
    int rc = SDL_GameControllerAddMappingsFromFile(path);
    if(-1 == rc) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyLong_FromLong(rc);
}

static PyObject * PySDL_GameControllerUpdate(PyObject *self, PyObject *ign) {
    SDL_GameControllerUpdate();
    Py_RETURN_NONE;
}

static PyObject * PySDL_GameControllerEventState(PyObject *self, PyObject *args) {
    int state = SDL_QUERY;
    if(!PyArg_ParseTuple(args, "|i", &state)) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GameControllerEventState(state));
}

PyMethodDef pysdl_gamecontroller_methods[] = {
    { "IsGameController",                 PySDL_IsGameController,                 METH_O       },
    { "GameControllerNameForIndex",       PySDL_GameControllerNameForIndex,       METH_O       },
    { "GameControllerAddMapping",         PySDL_GameControllerAddMapping,         METH_O       },
    { "GameControllerAddMappingsFromFile", PySDL_GameControllerAddMappingsFromFile, METH_O     },
    { "GameControllerUpdate",             PySDL_GameControllerUpdate,             METH_NOARGS  },
    { "GameControllerEventState",         PySDL_GameControllerEventState,         METH_VARARGS },
    { NULL }
};
