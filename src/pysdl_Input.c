#include "pysdl.h"

// Module-level keyboard, mouse, and text-input functions. Registered in
// PyInit_SDL2 with PyModule_AddFunctions(module, pysdl_input_methods).

//=========================================================
// keyboard
//=========================================================

static PyObject * PySDL_GetKeyName(PyObject *self, PyObject *arg) {
    long key = PyLong_AsLong(arg);
    if(-1 == key && PyErr_Occurred()) {
        return NULL;
    }
    return PyUnicode_FromString(SDL_GetKeyName((SDL_Keycode)key));
}

static PyObject * PySDL_GetKeyFromName(PyObject *self, PyObject *arg) {
    const char *name = PyUnicode_AsUTF8(arg);
    if(NULL == name) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetKeyFromName(name));
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
    return PyLong_FromLong(SDL_GetKeyFromScancode((SDL_Scancode)scancode));
}

static PyObject * PySDL_GetScancodeFromKey(PyObject *self, PyObject *arg) {
    long key = PyLong_AsLong(arg);
    if(-1 == key && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetScancodeFromKey((SDL_Keycode)key));
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
    const Uint8 *keys = SDL_GetKeyboardState(&len);
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
        PyErr_SetString(PyExc_TypeError, "expected an SDL2.Window");
        return NULL;
    }
    return PyBool_FromLong(SDL_IsScreenKeyboardShown(((PySDL_Window *)arg)->window));
}

//=========================================================
// text input
//=========================================================

static PyObject * PySDL_StartTextInput(PyObject *self, PyObject *ign) {
    SDL_StartTextInput();
    Py_RETURN_NONE;
}

static PyObject * PySDL_StopTextInput(PyObject *self, PyObject *ign) {
    SDL_StopTextInput();
    Py_RETURN_NONE;
}

static PyObject * PySDL_IsTextInputActive(PyObject *self, PyObject *ign) {
    return PyBool_FromLong(SDL_IsTextInputActive());
}

static PyObject * PySDL_SetTextInputRect(PyObject *self, PyObject *arg) {
    SDL_Rect rect;
    SDL_Rect *rp = NULL;
    if(arg != Py_None) {
        if(!PyToRect(arg, &rect)) {
            return NULL;
        }
        rp = &rect;
    }
    SDL_SetTextInputRect(rp);
    Py_RETURN_NONE;
}

//=========================================================
// mouse
//=========================================================

static PyObject * PySDL_GetMouseState(PyObject *self, PyObject *ign) {
    int x = 0, y = 0;
    Uint32 buttons = SDL_GetMouseState(&x, &y);
    return Py_BuildValue("(Iii)", buttons, x, y);
}

static PyObject * PySDL_GetGlobalMouseState(PyObject *self, PyObject *ign) {
    int x = 0, y = 0;
    Uint32 buttons = SDL_GetGlobalMouseState(&x, &y);
    return Py_BuildValue("(Iii)", buttons, x, y);
}

static PyObject * PySDL_GetRelativeMouseState(PyObject *self, PyObject *ign) {
    int x = 0, y = 0;
    Uint32 buttons = SDL_GetRelativeMouseState(&x, &y);
    return Py_BuildValue("(Iii)", buttons, x, y);
}

static PyObject * PySDL_WarpMouseInWindow(PyObject *self, PyObject *args) {
    PyObject *window_py;
    int x, y;
    if(!PyArg_ParseTuple(args, "Oii", &window_py, &x, &y)) {
        return NULL;
    }

    SDL_Window *window = NULL;
    if(window_py != Py_None) {
        if(!PyObject_TypeCheck(window_py, &PySDL_Window_Type)) {
            PyErr_SetString(PyExc_TypeError, "expected an SDL2.Window or None");
            return NULL;
        }
        window = ((PySDL_Window *)window_py)->window;
    }
    SDL_WarpMouseInWindow(window, x, y);
    Py_RETURN_NONE;
}

static PyObject * PySDL_WarpMouseGlobal(PyObject *self, PyObject *args) {
    int x, y;
    if(!PyArg_ParseTuple(args, "ii", &x, &y)) {
        return NULL;
    }
    if(0 > SDL_WarpMouseGlobal(x, y)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_SetRelativeMouseMode(PyObject *self, PyObject *arg) {
    int enable = PyObject_IsTrue(arg);
    if(-1 == enable) {
        return NULL;
    }
    if(0 > SDL_SetRelativeMouseMode(enable ? true : false)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_GetRelativeMouseMode(PyObject *self, PyObject *ign) {
    return PyBool_FromLong(SDL_GetRelativeMouseMode());
}

static PyObject * PySDL_CaptureMouse(PyObject *self, PyObject *arg) {
    int enable = PyObject_IsTrue(arg);
    if(-1 == enable) {
        return NULL;
    }
    if(0 > SDL_CaptureMouse(enable ? true : false)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_GetMouseFocus(PyObject *self, PyObject *ign) {
    return PySDL_WrapWindow(SDL_GetMouseFocus());
}

//=========================================================
// touch
//=========================================================

static PyObject * PySDL_GetNumTouchDevices(PyObject *self, PyObject *ign) {
    return PyLong_FromLong(SDL_GetNumTouchDevices());
}

static PyObject * PySDL_GetTouchDevice(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLongLong(SDL_GetTouchDevice((int)index));
}

static PyObject * PySDL_GetTouchDeviceType(PyObject *self, PyObject *arg) {
    long long touchID = PyLong_AsLongLong(arg);
    if(-1 == touchID && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetTouchDeviceType((SDL_TouchID)touchID));
}

#if SDL_VERSION_ATLEAST(2,0,22)
static PyObject * PySDL_GetTouchName(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_GetTouchName((int)index);
    return PyUnicode_FromString(name ? name : "");
}
#endif

static PyObject * PySDL_GetNumTouchFingers(PyObject *self, PyObject *arg) {
    long long touchID = PyLong_AsLongLong(arg);
    if(-1 == touchID && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetNumTouchFingers((SDL_TouchID)touchID));
}

static PyObject * PySDL_GetTouchFinger(PyObject *self, PyObject *args) {
    long long touchID;
    int index;
    if(!PyArg_ParseTuple(args, "Li", &touchID, &index)) {
        return NULL;
    }
    SDL_Finger *finger = SDL_GetTouchFinger((SDL_TouchID)touchID, index);
    if(NULL == finger) {
        Py_RETURN_NONE;
    }
    return Py_BuildValue("(Lfff)", (long long)finger->id, finger->x, finger->y, finger->pressure);
}

static PyObject * PySDL_RecordGesture(PyObject *self, PyObject *arg) {
    long long touchID = PyLong_AsLongLong(arg);
    if(-1 == touchID && PyErr_Occurred()) {
        return NULL;
    }
    return PyBool_FromLong(SDL_RecordGesture((SDL_TouchID)touchID));
}

static PyObject * PySDL_LoadDollarTemplates(PyObject *self, PyObject *args) {
    long long touchID;
    const char *path;
    if(!PyArg_ParseTuple(args, "Ls", &touchID, &path)) {
        return NULL;
    }
    SDL_RWops *rw = SDL_RWFromFile(path, "rb");
    if(NULL == rw) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    int rc = SDL_LoadDollarTemplates((SDL_TouchID)touchID, rw);
    SDL_RWclose(rw);
    if(0 > rc) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyLong_FromLong(rc);
}

static PyObject * PySDL_SaveDollarTemplate(PyObject *self, PyObject *args) {
    long long gestureID;
    const char *path;
    if(!PyArg_ParseTuple(args, "Ls", &gestureID, &path)) {
        return NULL;
    }
    SDL_RWops *rw = SDL_RWFromFile(path, "wb");
    if(NULL == rw) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    int rc = SDL_SaveDollarTemplate((SDL_GestureID)gestureID, rw);
    SDL_RWclose(rw);
    if(0 == rc) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_SaveAllDollarTemplates(PyObject *self, PyObject *arg) {
    const char *path = PyUnicode_AsUTF8(arg);
    if(NULL == path) {
        return NULL;
    }
    SDL_RWops *rw = SDL_RWFromFile(path, "wb");
    if(NULL == rw) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    int rc = SDL_SaveAllDollarTemplates(rw);
    SDL_RWclose(rw);
    if(0 == rc) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyLong_FromLong(rc);
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

    { "StartTextInput",           PySDL_StartTextInput,          METH_NOARGS },
    { "StopTextInput",            PySDL_StopTextInput,           METH_NOARGS },
    { "IsTextInputActive",        PySDL_IsTextInputActive,       METH_NOARGS },
    { "SetTextInputRect",         PySDL_SetTextInputRect,        METH_O      },

    { "GetMouseState",            PySDL_GetMouseState,           METH_NOARGS },
    { "GetGlobalMouseState",      PySDL_GetGlobalMouseState,     METH_NOARGS },
    { "GetRelativeMouseState",    PySDL_GetRelativeMouseState,   METH_NOARGS },
    { "WarpMouseInWindow",        PySDL_WarpMouseInWindow,       METH_VARARGS },
    { "WarpMouseGlobal",          PySDL_WarpMouseGlobal,         METH_VARARGS },
    { "SetRelativeMouseMode",     PySDL_SetRelativeMouseMode,    METH_O      },
    { "GetRelativeMouseMode",     PySDL_GetRelativeMouseMode,    METH_NOARGS },
    { "CaptureMouse",             PySDL_CaptureMouse,            METH_O      },
    { "GetMouseFocus",            PySDL_GetMouseFocus,           METH_NOARGS },

    { "GetNumTouchDevices",       PySDL_GetNumTouchDevices,      METH_NOARGS },
    { "GetTouchDevice",           PySDL_GetTouchDevice,          METH_O      },
    { "GetTouchDeviceType",       PySDL_GetTouchDeviceType,      METH_O      },
#if SDL_VERSION_ATLEAST(2,0,22)
    { "GetTouchName",             PySDL_GetTouchName,            METH_O      },
#endif
    { "GetNumTouchFingers",       PySDL_GetNumTouchFingers,      METH_O      },
    { "GetTouchFinger",           PySDL_GetTouchFinger,          METH_VARARGS },
    { "RecordGesture",            PySDL_RecordGesture,           METH_O      },
    { "LoadDollarTemplates",      PySDL_LoadDollarTemplates,     METH_VARARGS },
    { "SaveDollarTemplate",       PySDL_SaveDollarTemplate,      METH_VARARGS },
    { "SaveAllDollarTemplates",   PySDL_SaveAllDollarTemplates,  METH_O      },

    { NULL }
};
