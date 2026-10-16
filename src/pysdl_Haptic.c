#include "pysdl.h"

static int        PySDL_Haptic_Type_init    (PySDL_Haptic*, PyObject*, PyObject*);
static void       PySDL_Haptic_Type_dealloc (PySDL_Haptic*);

static PyObject * PySDL_Haptic_Index             (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_Query             (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_NumAxes           (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_NumEffects        (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_NumEffectsPlaying (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_RumbleSupported   (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_RumbleInit        (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_RumblePlay        (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_RumbleStop        (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_SetGain           (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_SetAutocenter     (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_Pause             (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_Unpause           (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_StopAll           (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_EffectSupported   (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_NewEffect         (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_UpdateEffect      (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_RunEffect         (PySDL_Haptic*, PyObject*, PyObject*);
static PyObject * PySDL_Haptic_StopEffect        (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_DestroyEffect     (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_GetEffectStatus   (PySDL_Haptic*, PyObject*);
static PyObject * PySDL_Haptic_Close             (PySDL_Haptic*, PyObject*);

static PyMethodDef PySDL_Haptic_methods[] = {
    { "Index",             (PyCFunction)PySDL_Haptic_Index,             METH_NOARGS },
    { "Query",             (PyCFunction)PySDL_Haptic_Query,             METH_NOARGS },
    { "NumAxes",           (PyCFunction)PySDL_Haptic_NumAxes,           METH_NOARGS },
    { "NumEffects",        (PyCFunction)PySDL_Haptic_NumEffects,        METH_NOARGS },
    { "NumEffectsPlaying", (PyCFunction)PySDL_Haptic_NumEffectsPlaying, METH_NOARGS },
    { "RumbleSupported",   (PyCFunction)PySDL_Haptic_RumbleSupported,   METH_NOARGS },
    { "RumbleInit",        (PyCFunction)PySDL_Haptic_RumbleInit,        METH_NOARGS },
    { "RumblePlay",        (PyCFunction)PySDL_Haptic_RumblePlay,        METH_VARARGS },
    { "RumbleStop",        (PyCFunction)PySDL_Haptic_RumbleStop,        METH_NOARGS },
    { "SetGain",           (PyCFunction)PySDL_Haptic_SetGain,           METH_O      },
    { "SetAutocenter",     (PyCFunction)PySDL_Haptic_SetAutocenter,     METH_O      },
    { "Pause",             (PyCFunction)PySDL_Haptic_Pause,             METH_NOARGS },
    { "Unpause",           (PyCFunction)PySDL_Haptic_Unpause,           METH_NOARGS },
    { "StopAll",           (PyCFunction)PySDL_Haptic_StopAll,           METH_NOARGS },
    { "EffectSupported",   (PyCFunction)PySDL_Haptic_EffectSupported,   METH_O      },
    { "NewEffect",         (PyCFunction)PySDL_Haptic_NewEffect,         METH_O      },
    { "UpdateEffect",      (PyCFunction)PySDL_Haptic_UpdateEffect,      METH_VARARGS },
    { "RunEffect",         (PyCFunction)PySDL_Haptic_RunEffect,         METH_VARARGS | METH_KEYWORDS },
    { "StopEffect",        (PyCFunction)PySDL_Haptic_StopEffect,        METH_O      },
    { "DestroyEffect",     (PyCFunction)PySDL_Haptic_DestroyEffect,     METH_O      },
    { "GetEffectStatus",   (PyCFunction)PySDL_Haptic_GetEffectStatus,   METH_O      },
    { "Close",             (PyCFunction)PySDL_Haptic_Close,             METH_NOARGS },
    { NULL }
};

PyTypeObject PySDL_Haptic_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Haptic",
    .tp_basicsize = sizeof(PySDL_Haptic),
    .tp_dealloc   = (destructor)PySDL_Haptic_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Haptic(device_index)",
    .tp_methods   = PySDL_Haptic_methods,
    .tp_init      = (initproc)PySDL_Haptic_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_Haptic_Type_init(PySDL_Haptic *self, PyObject *args, PyObject *kwds) {
    long long id = -1;
    if(!PyArg_ParseTuple(args, "|L", &id)) {
        return -1;
    }
    self->haptic = NULL;
    if(id > 0) {
        self->haptic = SDL_OpenHaptic((SDL_HapticID)id);
        if(NULL == self->haptic) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_Haptic_Type_dealloc(PySDL_Haptic *self) {
    if(NULL != self->haptic) {
        SDL_CloseHaptic(self->haptic);
        self->haptic = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static SDL_Haptic * _h(PySDL_Haptic *self) {
    if(NULL == self->haptic) {
        PyErr_SetString(pysdl_Error, "Haptic device is not open");
        return NULL;
    }
    return self->haptic;
}

static PyObject * _raise(void) {
    PyErr_SetString(pysdl_Error, SDL_GetError());
    return NULL;
}

// dict -> SDL_HapticEffect for the LEFTRIGHT / CONSTANT / periodic families.
static long _get(PyObject *d, const char *key, long fallback) {
    PyObject *v = PyDict_GetItemString(d, key);  // borrowed, no exception if missing
    if(NULL == v) {
        return fallback;
    }
    long n = PyLong_AsLong(v);
    if(-1 == n && PyErr_Occurred()) {
        PyErr_Clear();
        return fallback;
    }
    return n;
}

static int _dict_to_effect(PyObject *dict, SDL_HapticEffect *effect) {
    if(!PyDict_Check(dict)) {
        PyErr_SetString(PyExc_TypeError, "effect must be a dict with a 'type' key");
        return 0;
    }
    PyObject *type_obj = PyDict_GetItemString(dict, "type");
    if(NULL == type_obj) {
        PyErr_SetString(PyExc_KeyError, "effect dict needs a 'type'");
        return 0;
    }
    long type = PyLong_AsLong(type_obj);
    if(-1 == type && PyErr_Occurred()) {
        return 0;
    }

    SDL_memset(effect, 0, sizeof(*effect));

    switch(type) {
    case SDL_HAPTIC_LEFTRIGHT:
        effect->leftright.type = SDL_HAPTIC_LEFTRIGHT;
        effect->leftright.length          = (Uint32)_get(dict, "length", 1000);
        effect->leftright.large_magnitude = (Uint16)_get(dict, "large_magnitude", 0);
        effect->leftright.small_magnitude = (Uint16)_get(dict, "small_magnitude", 0);
        return 1;

    case SDL_HAPTIC_CONSTANT:
        effect->constant.type = SDL_HAPTIC_CONSTANT;
        effect->constant.direction.type = SDL_HAPTIC_POLAR;
        effect->constant.direction.dir[0] = _get(dict, "direction", 0);
        effect->constant.length        = (Uint32)_get(dict, "length", 1000);
        effect->constant.delay         = (Uint16)_get(dict, "delay", 0);
        effect->constant.level         = (Sint16)_get(dict, "level", 0x4000);
        effect->constant.attack_length = (Uint16)_get(dict, "attack_length", 0);
        effect->constant.attack_level  = (Uint16)_get(dict, "attack_level", 0);
        effect->constant.fade_length   = (Uint16)_get(dict, "fade_length", 0);
        effect->constant.fade_level    = (Uint16)_get(dict, "fade_level", 0);
        return 1;

    case SDL_HAPTIC_SINE:
    case SDL_HAPTIC_TRIANGLE:
    case SDL_HAPTIC_SAWTOOTHUP:
    case SDL_HAPTIC_SAWTOOTHDOWN:
        effect->periodic.type = (Uint16)type;
        effect->periodic.direction.type = SDL_HAPTIC_POLAR;
        effect->periodic.direction.dir[0] = _get(dict, "direction", 0);
        effect->periodic.length        = (Uint32)_get(dict, "length", 1000);
        effect->periodic.delay         = (Uint16)_get(dict, "delay", 0);
        effect->periodic.period        = (Uint16)_get(dict, "period", 100);
        effect->periodic.magnitude     = (Sint16)_get(dict, "magnitude", 0x4000);
        effect->periodic.offset        = (Sint16)_get(dict, "offset", 0);
        effect->periodic.phase         = (Uint16)_get(dict, "phase", 0);
        effect->periodic.attack_length = (Uint16)_get(dict, "attack_length", 0);
        effect->periodic.attack_level  = (Uint16)_get(dict, "attack_level", 0);
        effect->periodic.fade_length   = (Uint16)_get(dict, "fade_length", 0);
        effect->periodic.fade_level    = (Uint16)_get(dict, "fade_level", 0);
        return 1;
    }

    PyErr_SetString(pysdl_Error, "effect type not supported (LEFTRIGHT / CONSTANT / SINE / TRIANGLE / SAWTOOTH* only)");
    return 0;
}

static PyObject * PySDL_Haptic_Index(PySDL_Haptic *self, PyObject *ign) {
    SDL_Haptic *h = _h(self);
    return h ? PyLong_FromUnsignedLong(SDL_GetHapticID(h)) : NULL;
}

static PyObject * PySDL_Haptic_Query(PySDL_Haptic *self, PyObject *ign) {
    SDL_Haptic *h = _h(self);
    return h ? PyLong_FromUnsignedLong(SDL_GetHapticFeatures(h)) : NULL;
}

static PyObject * PySDL_Haptic_NumAxes(PySDL_Haptic *self, PyObject *ign) {
    SDL_Haptic *h = _h(self);
    return h ? PyLong_FromLong(SDL_GetNumHapticAxes(h)) : NULL;
}

static PyObject * PySDL_Haptic_NumEffects(PySDL_Haptic *self, PyObject *ign) {
    SDL_Haptic *h = _h(self);
    return h ? PyLong_FromLong(SDL_GetMaxHapticEffects(h)) : NULL;
}

static PyObject * PySDL_Haptic_NumEffectsPlaying(PySDL_Haptic *self, PyObject *ign) {
    SDL_Haptic *h = _h(self);
    return h ? PyLong_FromLong(SDL_GetMaxHapticEffectsPlaying(h)) : NULL;
}

static PyObject * PySDL_Haptic_RumbleSupported(PySDL_Haptic *self, PyObject *ign) {
    SDL_Haptic *h = _h(self);
    return h ? PyBool_FromLong(SDL_HapticRumbleSupported(h)) : NULL;
}

static PyObject * PySDL_Haptic_RumbleInit(PySDL_Haptic *self, PyObject *ign) {
    SDL_Haptic *h = _h(self);
    if(NULL == h) return NULL;
    if(!SDL_InitHapticRumble(h)) return _raise();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_RumblePlay(PySDL_Haptic *self, PyObject *args) {
    float strength;
    unsigned int length;
    SDL_Haptic *h = _h(self);
    if(NULL == h || !PyArg_ParseTuple(args, "fI", &strength, &length)) {
        return NULL;
    }
    if(!SDL_PlayHapticRumble(h, strength, length)) return _raise();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_RumbleStop(PySDL_Haptic *self, PyObject *ign) {
    SDL_Haptic *h = _h(self);
    if(NULL == h) return NULL;
    if(!SDL_StopHapticRumble(h)) return _raise();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_SetGain(PySDL_Haptic *self, PyObject *arg) {
    SDL_Haptic *h = _h(self);
    long gain = PyLong_AsLong(arg);
    if(NULL == h || (-1 == gain && PyErr_Occurred())) return NULL;
    if(!SDL_SetHapticGain(h, (int)gain)) return _raise();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_SetAutocenter(PySDL_Haptic *self, PyObject *arg) {
    SDL_Haptic *h = _h(self);
    long pct = PyLong_AsLong(arg);
    if(NULL == h || (-1 == pct && PyErr_Occurred())) return NULL;
    if(!SDL_SetHapticAutocenter(h, (int)pct)) return _raise();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_Pause(PySDL_Haptic *self, PyObject *ign) {
    SDL_Haptic *h = _h(self);
    if(NULL == h) return NULL;
    if(!SDL_PauseHaptic(h)) return _raise();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_Unpause(PySDL_Haptic *self, PyObject *ign) {
    SDL_Haptic *h = _h(self);
    if(NULL == h) return NULL;
    if(!SDL_ResumeHaptic(h)) return _raise();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_StopAll(PySDL_Haptic *self, PyObject *ign) {
    SDL_Haptic *h = _h(self);
    if(NULL == h) return NULL;
    if(!SDL_StopHapticEffects(h)) return _raise();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_EffectSupported(PySDL_Haptic *self, PyObject *arg) {
    SDL_Haptic *h = _h(self);
    if(NULL == h) return NULL;
    SDL_HapticEffect effect;
    if(!_dict_to_effect(arg, &effect)) return NULL;
    return PyBool_FromLong(SDL_HapticEffectSupported(h, &effect));
}

static PyObject * PySDL_Haptic_NewEffect(PySDL_Haptic *self, PyObject *arg) {
    SDL_Haptic *h = _h(self);
    if(NULL == h) return NULL;
    SDL_HapticEffect effect;
    if(!_dict_to_effect(arg, &effect)) return NULL;
    int id = SDL_CreateHapticEffect(h, &effect);
    if(0 > id) return _raise();
    return PyLong_FromLong(id);
}

static PyObject * PySDL_Haptic_UpdateEffect(PySDL_Haptic *self, PyObject *args) {
    int effect_id;
    PyObject *dict;
    SDL_Haptic *h = _h(self);
    if(NULL == h || !PyArg_ParseTuple(args, "iO", &effect_id, &dict)) {
        return NULL;
    }
    SDL_HapticEffect effect;
    if(!_dict_to_effect(dict, &effect)) return NULL;
    if(!SDL_UpdateHapticEffect(h, effect_id, &effect)) return _raise();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_RunEffect(PySDL_Haptic *self, PyObject *args, PyObject *kwds) {
    int effect_id;
    unsigned int iterations = 1;
    SDL_Haptic *h = _h(self);
    static char *kwlist[] = {"effect_id", "iterations", NULL};
    if(NULL == h || !PyArg_ParseTupleAndKeywords(args, kwds, "i|I", kwlist, &effect_id, &iterations)) {
        return NULL;
    }
    if(!SDL_RunHapticEffect(h, effect_id, iterations)) return _raise();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_StopEffect(PySDL_Haptic *self, PyObject *arg) {
    SDL_Haptic *h = _h(self);
    long effect_id = PyLong_AsLong(arg);
    if(NULL == h || (-1 == effect_id && PyErr_Occurred())) return NULL;
    if(!SDL_StopHapticEffect(h, (int)effect_id)) return _raise();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_DestroyEffect(PySDL_Haptic *self, PyObject *arg) {
    SDL_Haptic *h = _h(self);
    long effect_id = PyLong_AsLong(arg);
    if(NULL == h || (-1 == effect_id && PyErr_Occurred())) return NULL;
    SDL_DestroyHapticEffect(h, (int)effect_id);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Haptic_GetEffectStatus(PySDL_Haptic *self, PyObject *arg) {
    SDL_Haptic *h = _h(self);
    long effect_id = PyLong_AsLong(arg);
    if(NULL == h || (-1 == effect_id && PyErr_Occurred())) return NULL;
    return PyBool_FromLong(SDL_GetHapticEffectStatus(h, (int)effect_id));
}

static PyObject * PySDL_Haptic_Close(PySDL_Haptic *self, PyObject *ign) {
    if(NULL != self->haptic) {
        SDL_CloseHaptic(self->haptic);
        self->haptic = NULL;
    }
    Py_RETURN_NONE;
}

//=========================================================
// module functions
//=========================================================

static PyObject * PySDL_NumHaptics(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_HapticID *ids = SDL_GetHaptics(&count);
    SDL_free(ids);
    return PyLong_FromLong(count);
}

static PyObject * PySDL_GetHaptics(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_HapticID *ids = SDL_GetHaptics(&count);
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

static PyObject * PySDL_HapticName(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_GetHapticNameForID((SDL_HapticID)id);
    return PyUnicode_FromString(name ? name : "");
}

// HapticOpened(id) -> whether that haptic device is open.
static PyObject * PySDL_HapticOpened(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    return PyBool_FromLong(NULL != SDL_GetHapticFromID((SDL_HapticID)id));
}

// GetHapticFromID(id) -> a new, owned Haptic for an already-open device
// (re-opening by ID bumps SDL's refcount), or None. A haptic opened from a
// joystick may not be re-openable by ID; that raises SDL's error.
static PyObject * PySDL_GetHapticFromID(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    if(NULL == SDL_GetHapticFromID((SDL_HapticID)id)) {
        Py_RETURN_NONE;
    }
    return PyObject_CallFunction((PyObject *)&PySDL_Haptic_Type, "k", id);
}

static PyObject * PySDL_MouseIsHaptic(PyObject *self, PyObject *ign) {
    return PyBool_FromLong(SDL_IsMouseHaptic());
}

static PyObject * PySDL_HapticOpenFromMouse(PyObject *self, PyObject *ign) {
    PySDL_Haptic *wrapper = (PySDL_Haptic *)PySDL_New(&PySDL_Haptic_Type);
    if(NULL == wrapper) return NULL;
    wrapper->haptic = SDL_OpenHapticFromMouse();
    if(NULL == wrapper->haptic) {
        Py_DECREF(wrapper);
        return _raise();
    }
    return (PyObject *)wrapper;
}

// The two joystick-based calls take SDL's joystick lock: see PySDL_JoystickLock.
static PyObject * PySDL_JoystickIsHaptic(PyObject *self, PyObject *arg) {
    if(!PyObject_TypeCheck(arg, &PySDL_Joystick_Type)) {
        PyErr_SetString(PyExc_TypeError, "expected an SDL3.Joystick");
        return NULL;
    }
    int locked = PySDL_JoystickLock();
    bool haptic = SDL_IsJoystickHaptic(((PySDL_Joystick *)arg)->joystick);
    PySDL_JoystickUnlock(locked);
    return PyBool_FromLong(haptic);
}

static PyObject * PySDL_HapticOpenFromJoystick(PyObject *self, PyObject *arg) {
    if(!PyObject_TypeCheck(arg, &PySDL_Joystick_Type)) {
        PyErr_SetString(PyExc_TypeError, "expected an SDL3.Joystick");
        return NULL;
    }
    PySDL_Haptic *wrapper = (PySDL_Haptic *)PySDL_New(&PySDL_Haptic_Type);
    if(NULL == wrapper) return NULL;
    int locked = PySDL_JoystickLock();
    wrapper->haptic = SDL_OpenHapticFromJoystick(((PySDL_Joystick *)arg)->joystick);
    PySDL_JoystickUnlock(locked);
    if(NULL == wrapper->haptic) {
        Py_DECREF(wrapper);
        return _raise();
    }
    return (PyObject *)wrapper;
}

PyMethodDef pysdl_haptic_methods[] = {
    { "NumHaptics",              PySDL_NumHaptics,              METH_NOARGS },
    { "GetHaptics",              PySDL_GetHaptics,              METH_NOARGS },
    { "HapticName",              PySDL_HapticName,              METH_O      },
    { "GetHapticNameForID",      PySDL_HapticName,              METH_O      },
    { "HapticOpened",            PySDL_HapticOpened,            METH_O      },
    { "GetHapticFromID",         PySDL_GetHapticFromID,         METH_O      },
    { "MouseIsHaptic",           PySDL_MouseIsHaptic,           METH_NOARGS },
    { "HapticOpenFromMouse",     PySDL_HapticOpenFromMouse,     METH_NOARGS },
    { "JoystickIsHaptic",        PySDL_JoystickIsHaptic,        METH_O      },
    { "HapticOpenFromJoystick",  PySDL_HapticOpenFromJoystick,  METH_O      },
    { NULL }
};
