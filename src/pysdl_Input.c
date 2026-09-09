#include "pysdl.h"

// Module-level keyboard, mouse, and text-input functions. Registered in
// PyInit_SDL3 with PyModule_AddFunctions(module, pysdl_input_methods).

// Optional trailing SDL3.Window argument -> SDL_Window * (or NULL / error).
static int _opt_window(PyObject *obj, SDL_Window **out) {
    *out = NULL;
    if(NULL == obj || obj == Py_None) {
        return 1;
    }
    if(!PyObject_TypeCheck(obj, &PySDL_Window_Type)) {
        PyErr_SetString(PyExc_TypeError, "expected an SDL3.Window or None");
        return 0;
    }
    *out = ((PySDL_Window *)obj)->window;
    return 1;
}

//=========================================================
// keyboard
//=========================================================

static PyObject * PySDL_GetKeyName(PyObject *self, PyObject *arg) {
    unsigned long key = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == key && PyErr_Occurred()) {
        return NULL;
    }
    return PyUnicode_FromString(SDL_GetKeyName((SDL_Keycode)key));
}

static PyObject * PySDL_GetKeyFromName(PyObject *self, PyObject *arg) {
    const char *name = PyUnicode_AsUTF8(arg);
    if(NULL == name) {
        return NULL;
    }
    return PyLong_FromUnsignedLong(SDL_GetKeyFromName(name));
}

static PyObject * PySDL_GetScancodeName(PyObject *self, PyObject *arg) {
    long scancode = PyLong_AsLong(arg);
    if(-1 == scancode && PyErr_Occurred()) {
        return NULL;
    }
    return PyUnicode_FromString(SDL_GetScancodeName((SDL_Scancode)scancode));
}

static PyObject * PySDL_GetScancodeFromName(PyObject *self, PyObject *arg) {
    const char *name = PyUnicode_AsUTF8(arg);
    if(NULL == name) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetScancodeFromName(name));
}

static PyObject * PySDL_GetKeyFromScancode(PyObject *self, PyObject *arg) {
    long scancode = PyLong_AsLong(arg);
    if(-1 == scancode && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromUnsignedLong(SDL_GetKeyFromScancode((SDL_Scancode)scancode, SDL_KMOD_NONE, false));
}

static PyObject * PySDL_GetScancodeFromKey(PyObject *self, PyObject *arg) {
    unsigned long key = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == key && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetScancodeFromKey((SDL_Keycode)key, NULL));
}

static PyObject * PySDL_SetModState(PyObject *self, PyObject *arg) {
    long mod = PyLong_AsLong(arg);
    if(-1 == mod && PyErr_Occurred()) {
        return NULL;
    }
    SDL_SetModState((SDL_Keymod)mod);
    Py_RETURN_NONE;
}

static PyObject * PySDL_GetModState(PyObject *self, PyObject *ign) {
    return PyLong_FromLong(SDL_GetModState());
}

static PyObject * PySDL_GetKeyState(PyObject *self, PyObject *ign) {
    int len = 0;
    const bool *keys = SDL_GetKeyboardState(&len);
    PyObject *list = PyList_New(len);
    if(NULL == list) {
        return NULL;
    }
    for(int idx = 0; idx < len; ++idx) {
        PyList_SET_ITEM(list, idx, PyBool_FromLong(keys[idx]));
    }
    return list;
}

static PyObject * PySDL_GetKeyboardFocus(PyObject *self, PyObject *ign) {
    return PySDL_WrapWindow(SDL_GetKeyboardFocus());
}

static PyObject * PySDL_HasScreenKeyboardSupport(PyObject *self, PyObject *ign) {
    return PyBool_FromLong(SDL_HasScreenKeyboardSupport());
}

static PyObject * PySDL_IsScreenKeyboardShown(PyObject *self, PyObject *arg) {
    if(!PyObject_TypeCheck(arg, &PySDL_Window_Type)) {
        PyErr_SetString(PyExc_TypeError, "expected an SDL3.Window");
        return NULL;
    }
    return PyBool_FromLong(SDL_ScreenKeyboardShown(((PySDL_Window *)arg)->window));
}

//=========================================================
// text input  (SDL3: per-window)
//=========================================================

static PyObject * PySDL_StartTextInput(PyObject *self, PyObject *arg) {
    SDL_Window *window;
    if(!_opt_window(arg, &window) || NULL == window) {
        if(!PyErr_Occurred()) {
            PyErr_SetString(PyExc_TypeError, "StartTextInput requires a Window");
        }
        return NULL;
    }
    if(!SDL_StartTextInput(window)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_StopTextInput(PyObject *self, PyObject *arg) {
    SDL_Window *window;
    if(!_opt_window(arg, &window) || NULL == window) {
        if(!PyErr_Occurred()) {
            PyErr_SetString(PyExc_TypeError, "StopTextInput requires a Window");
        }
        return NULL;
    }
    SDL_StopTextInput(window);
    Py_RETURN_NONE;
}

static PyObject * PySDL_IsTextInputActive(PyObject *self, PyObject *arg) {
    SDL_Window *window;
    if(!_opt_window(arg, &window) || NULL == window) {
        if(!PyErr_Occurred()) {
            PyErr_SetString(PyExc_TypeError, "IsTextInputActive requires a Window");
        }
        return NULL;
    }
    return PyBool_FromLong(SDL_TextInputActive(window));
}

static PyObject * PySDL_SetTextInputRect(PyObject *self, PyObject *args) {
    PyObject *window_py;
    PyObject *rect_py;
    int cursor = 0;
    if(!PyArg_ParseTuple(args, "OO|i", &window_py, &rect_py, &cursor)) {
        return NULL;
    }
    SDL_Window *window;
    if(!_opt_window(window_py, &window) || NULL == window) {
        if(!PyErr_Occurred()) {
            PyErr_SetString(PyExc_TypeError, "SetTextInputArea requires a Window");
        }
        return NULL;
    }
    SDL_Rect rect;
    SDL_Rect *rp = NULL;
    if(rect_py != Py_None) {
        if(!PyToRect(rect_py, &rect)) {
            return NULL;
        }
        rp = &rect;
    }
    SDL_SetTextInputArea(window, rp, cursor);
    Py_RETURN_NONE;
}

//=========================================================
// mouse  (SDL3: float coordinates)
//=========================================================

static PyObject * PySDL_GetMouseState(PyObject *self, PyObject *ign) {
    float x = 0, y = 0;
    SDL_MouseButtonFlags buttons = SDL_GetMouseState(&x, &y);
    return Py_BuildValue("(Iff)", buttons, x, y);
}

static PyObject * PySDL_GetGlobalMouseState(PyObject *self, PyObject *ign) {
    float x = 0, y = 0;
    SDL_MouseButtonFlags buttons = SDL_GetGlobalMouseState(&x, &y);
    return Py_BuildValue("(Iff)", buttons, x, y);
}

static PyObject * PySDL_GetRelativeMouseState(PyObject *self, PyObject *ign) {
    float x = 0, y = 0;
    SDL_MouseButtonFlags buttons = SDL_GetRelativeMouseState(&x, &y);
    return Py_BuildValue("(Iff)", buttons, x, y);
}

static PyObject * PySDL_WarpMouseInWindow(PyObject *self, PyObject *args) {
    PyObject *window_py;
    float x, y;
    if(!PyArg_ParseTuple(args, "Off", &window_py, &x, &y)) {
        return NULL;
    }
    SDL_Window *window;
    if(!_opt_window(window_py, &window)) {
        return NULL;
    }
    SDL_WarpMouseInWindow(window, x, y);
    Py_RETURN_NONE;
}

static PyObject * PySDL_WarpMouseGlobal(PyObject *self, PyObject *args) {
    float x, y;
    if(!PyArg_ParseTuple(args, "ff", &x, &y)) {
        return NULL;
    }
    if(!SDL_WarpMouseGlobal(x, y)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

// SDL3: relative mouse mode is per-window.
static PyObject * PySDL_SetRelativeMouseMode(PyObject *self, PyObject *args) {
    PyObject *window_py;
    int enable;
    if(!PyArg_ParseTuple(args, "Op", &window_py, &enable)) {
        return NULL;
    }
    SDL_Window *window;
    if(!_opt_window(window_py, &window) || NULL == window) {
        if(!PyErr_Occurred()) {
            PyErr_SetString(PyExc_TypeError, "SetRelativeMouseMode requires a Window");
        }
        return NULL;
    }
    if(!SDL_SetWindowRelativeMouseMode(window, enable ? true : false)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_GetRelativeMouseMode(PyObject *self, PyObject *arg) {
    SDL_Window *window;
    if(!_opt_window(arg, &window) || NULL == window) {
        if(!PyErr_Occurred()) {
            PyErr_SetString(PyExc_TypeError, "GetRelativeMouseMode requires a Window");
        }
        return NULL;
    }
    return PyBool_FromLong(SDL_GetWindowRelativeMouseMode(window));
}

static PyObject * PySDL_CaptureMouse(PyObject *self, PyObject *arg) {
    int enable = PyObject_IsTrue(arg);
    if(-1 == enable) {
        return NULL;
    }
    if(!SDL_CaptureMouse(enable ? true : false)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_GetMouseFocus(PyObject *self, PyObject *ign) {
    return PySDL_WrapWindow(SDL_GetMouseFocus());
}

//=========================================================
// touch  (SDL3: SDL_TouchID lists; the gesture API is gone)
//=========================================================

static SDL_TouchID * _touch_devices(int *count) {
    return SDL_GetTouchDevices(count);
}

static PyObject * PySDL_GetNumTouchDevices(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_TouchID *ids = _touch_devices(&count);
    SDL_free(ids);
    return PyLong_FromLong(count);
}

static PyObject * PySDL_GetTouchDevice(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    int count = 0;
    SDL_TouchID *ids = _touch_devices(&count);
    if(NULL == ids || index < 0 || index >= count) {
        SDL_free(ids);
        PyErr_SetString(PyExc_IndexError, "touch device index out of range");
        return NULL;
    }
    SDL_TouchID id = ids[index];
    SDL_free(ids);
    return PyLong_FromUnsignedLongLong(id);
}

static PyObject * PySDL_GetTouchDeviceType(PyObject *self, PyObject *arg) {
    unsigned long long touchID = PyLong_AsUnsignedLongLong(arg);
    if((unsigned long long)-1 == touchID && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetTouchDeviceType((SDL_TouchID)touchID));
}

static PyObject * PySDL_GetTouchName(PyObject *self, PyObject *arg) {
    unsigned long long touchID = PyLong_AsUnsignedLongLong(arg);
    if((unsigned long long)-1 == touchID && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_GetTouchDeviceName((SDL_TouchID)touchID);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_GetNumTouchFingers(PyObject *self, PyObject *arg) {
    unsigned long long touchID = PyLong_AsUnsignedLongLong(arg);
    if((unsigned long long)-1 == touchID && PyErr_Occurred()) {
        return NULL;
    }
    int count = 0;
    SDL_Finger **fingers = SDL_GetTouchFingers((SDL_TouchID)touchID, &count);
    SDL_free(fingers);
    return PyLong_FromLong(count);
}

static PyObject * PySDL_GetTouchFinger(PyObject *self, PyObject *args) {
    unsigned long long touchID;
    int index;
    if(!PyArg_ParseTuple(args, "Ki", &touchID, &index)) {
        return NULL;
    }
    int count = 0;
    SDL_Finger **fingers = SDL_GetTouchFingers((SDL_TouchID)touchID, &count);
    if(NULL == fingers || index < 0 || index >= count) {
        SDL_free(fingers);
        Py_RETURN_NONE;
    }
    SDL_Finger *f = fingers[index];
    PyObject *result = Py_BuildValue("(Lfff)", (long long)f->id, f->x, f->y, f->pressure);
    SDL_free(fingers);
    return result;
}

PyMethodDef pysdl_input_methods[] = {
    { "GetKeyName",               PySDL_GetKeyName,               METH_O      },
    { "GetKeyFromName",           PySDL_GetKeyFromName,           METH_O      },
    { "GetScancodeName",          PySDL_GetScancodeName,          METH_O      },
    { "GetScancodeFromName",      PySDL_GetScancodeFromName,      METH_O      },
    { "GetKeyFromScancode",       PySDL_GetKeyFromScancode,       METH_O      },
    { "GetScancodeFromKey",       PySDL_GetScancodeFromKey,       METH_O      },
    { "SetModState",              PySDL_SetModState,              METH_O      },
    { "GetModState",              PySDL_GetModState,              METH_NOARGS },
    { "GetKeyState",              PySDL_GetKeyState,              METH_NOARGS },
    { "GetKeyboardFocus",         PySDL_GetKeyboardFocus,         METH_NOARGS },
    { "HasScreenKeyboardSupport", PySDL_HasScreenKeyboardSupport, METH_NOARGS },
    { "IsScreenKeyboardShown",    PySDL_IsScreenKeyboardShown,    METH_O      },

    { "StartTextInput",           PySDL_StartTextInput,          METH_O       },
    { "StopTextInput",            PySDL_StopTextInput,           METH_O       },
    { "IsTextInputActive",        PySDL_IsTextInputActive,       METH_O       },
    { "SetTextInputRect",         PySDL_SetTextInputRect,        METH_VARARGS },
    { "SetTextInputArea",         PySDL_SetTextInputRect,        METH_VARARGS },

    { "GetMouseState",            PySDL_GetMouseState,           METH_NOARGS  },
    { "GetGlobalMouseState",      PySDL_GetGlobalMouseState,     METH_NOARGS  },
    { "GetRelativeMouseState",    PySDL_GetRelativeMouseState,   METH_NOARGS  },
    { "WarpMouseInWindow",        PySDL_WarpMouseInWindow,       METH_VARARGS },
    { "WarpMouseGlobal",          PySDL_WarpMouseGlobal,         METH_VARARGS },
    { "SetRelativeMouseMode",     PySDL_SetRelativeMouseMode,    METH_VARARGS },
    { "GetRelativeMouseMode",     PySDL_GetRelativeMouseMode,    METH_O       },
    { "CaptureMouse",             PySDL_CaptureMouse,            METH_O       },
    { "GetMouseFocus",            PySDL_GetMouseFocus,           METH_NOARGS  },

    { "GetNumTouchDevices",       PySDL_GetNumTouchDevices,      METH_NOARGS  },
    { "GetTouchDevice",           PySDL_GetTouchDevice,          METH_O       },
    { "GetTouchDeviceType",       PySDL_GetTouchDeviceType,      METH_O       },
    { "GetTouchName",             PySDL_GetTouchName,            METH_O       },
    { "GetTouchDeviceName",       PySDL_GetTouchName,            METH_O       },
    { "GetNumTouchFingers",       PySDL_GetNumTouchFingers,      METH_O       },
    { "GetTouchFinger",           PySDL_GetTouchFinger,          METH_VARARGS },

    { NULL }
};
