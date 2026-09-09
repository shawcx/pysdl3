#include "pysdl.h"

// Module-level video functions: extra display queries, message boxes, hints,
// misc (OpenURL / locales), the GL and Vulkan loader entry points, and the
// borrowed-window lookups. Registered via PyModule_AddFunctions.

static PyObject * _raise(void) {
    PyErr_SetString(pysdl_Error, SDL_GetError());
    return NULL;
}

//=========================================================
// displays
//=========================================================

static PyObject * PySDL_GetDisplayName(PyObject *self, PyObject *arg) {
    long display = PyLong_AsLong(arg);
    if(-1 == display && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_GetDisplayName((int)display);
    if(NULL == name) {
        return _raise();
    }
    return PyUnicode_FromString(name);
}

static PyObject * PySDL_GetDisplayUsableBounds(PyObject *self, PyObject *arg) {
    long display = PyLong_AsLong(arg);
    if(-1 == display && PyErr_Occurred()) {
        return NULL;
    }
    SDL_Rect rect;
    if(0 > SDL_GetDisplayUsableBounds((int)display, &rect)) {
        return _raise();
    }
    return Py_BuildValue("(iiii)", rect.x, rect.y, rect.w, rect.h);
}

static PyObject * PySDL_GetDisplayOrientation(PyObject *self, PyObject *arg) {
    long display = PyLong_AsLong(arg);
    if(-1 == display && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetDisplayOrientation((int)display));
}

static PyObject * PySDL_GetNumDisplayModes(PyObject *self, PyObject *arg) {
    long display = PyLong_AsLong(arg);
    if(-1 == display && PyErr_Occurred()) {
        return NULL;
    }
    int count = SDL_GetNumDisplayModes((int)display);
    if(0 > count) {
        return _raise();
    }
    return PyLong_FromLong(count);
}

static PyObject * PySDL_GetClosestDisplayMode(PyObject *self, PyObject *args) {
    int display;
    SDL_DisplayMode want;
    SDL_memset(&want, 0, sizeof(want));
    if(!PyArg_ParseTuple(args, "i(iiii)", &display, &want.format, &want.w, &want.h, &want.refresh_rate)) {
        return NULL;
    }
    SDL_DisplayMode closest;
    if(NULL == SDL_GetClosestDisplayMode(display, &want, &closest)) {
        Py_RETURN_NONE;
    }
    return Py_BuildValue("(iiii)", closest.format, closest.w, closest.h, closest.refresh_rate);
}

#if SDL_VERSION_ATLEAST(2,24,0)
static PyObject * PySDL_GetPointDisplayIndex(PyObject *self, PyObject *arg) {
    SDL_Point point;
    if(!PyToPoint(arg, &point)) {
        return NULL;
    }
    int index = SDL_GetPointDisplayIndex(&point);
    if(0 > index) {
        return _raise();
    }
    return PyLong_FromLong(index);
}

static PyObject * PySDL_GetRectDisplayIndex(PyObject *self, PyObject *arg) {
    SDL_Rect rect;
    if(!PyToRect(arg, &rect)) {
        return NULL;
    }
    int index = SDL_GetRectDisplayIndex(&rect);
    if(0 > index) {
        return _raise();
    }
    return PyLong_FromLong(index);
}
#endif

//=========================================================
// borrowed-window lookups
//=========================================================

static PyObject * PySDL_GetWindowFromID(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    return PySDL_WrapWindow(SDL_GetWindowFromID((Uint32)id));
}

#if SDL_VERSION_ATLEAST(2,0,16)
static PyObject * PySDL_GetGrabbedWindow(PyObject *self, PyObject *ign) {
    return PySDL_WrapWindow(SDL_GetGrabbedWindow());
}
#endif

//=========================================================
// message boxes
//=========================================================

static PyObject * PySDL_ShowSimpleMessageBox(PyObject *self, PyObject *args, PyObject *kwds) {
    unsigned int flags = SDL_MESSAGEBOX_INFORMATION;
    const char *title;
    const char *message;
    PyObject *window_py = Py_None;

    static char *kwlist[] = {"title", "message", "flags", "window", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "ss|IO", kwlist,
        &title, &message, &flags, &window_py)) {
        return NULL;
    }

    SDL_Window *window = NULL;
    if(window_py != Py_None) {
        if(!PyObject_TypeCheck(window_py, &PySDL_Window_Type)) {
            PyErr_SetString(PyExc_TypeError, "window must be an SDL2.Window or None");
            return NULL;
        }
        window = ((PySDL_Window *)window_py)->window;
    }

    if(0 > SDL_ShowSimpleMessageBox(flags, title, message, window)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_ShowMessageBox(PyObject *self, PyObject *arg) {
    if(!PyDict_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "expected a dict {title, message, buttons, flags, window}");
        return NULL;
    }

    SDL_MessageBoxData data;
    SDL_memset(&data, 0, sizeof(data));

    PyObject *v;
    v = PyDict_GetItemString(arg, "flags");
    data.flags = v ? (Uint32)PyLong_AsUnsignedLong(v) : SDL_MESSAGEBOX_INFORMATION;
    v = PyDict_GetItemString(arg, "title");
    data.title = v ? PyUnicode_AsUTF8(v) : "";
    v = PyDict_GetItemString(arg, "message");
    data.message = v ? PyUnicode_AsUTF8(v) : "";
    if(PyErr_Occurred()) {
        return NULL;
    }

    PyObject *window_py = PyDict_GetItemString(arg, "window");
    if(window_py && window_py != Py_None) {
        if(!PyObject_TypeCheck(window_py, &PySDL_Window_Type)) {
            PyErr_SetString(PyExc_TypeError, "window must be an SDL2.Window or None");
            return NULL;
        }
        data.window = ((PySDL_Window *)window_py)->window;
    }

    PyObject *buttons_py = PyDict_GetItemString(arg, "buttons");
    if(NULL == buttons_py) {
        PyErr_SetString(PyExc_KeyError, "message box needs a 'buttons' list of (id, text[, flags])");
        return NULL;
    }
    PyObject *fast = PySequence_Fast(buttons_py, "'buttons' must be a list");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t nbuttons = PySequence_Fast_GET_SIZE(fast);
    SDL_MessageBoxButtonData *buttons = PyMem_New(SDL_MessageBoxButtonData, nbuttons > 0 ? nbuttons : 1);
    if(NULL == buttons) {
        Py_DECREF(fast);
        return PyErr_NoMemory();
    }
    for(Py_ssize_t idx = 0; idx < nbuttons; ++idx) {
        PyObject *b = PySequence_Fast_GET_ITEM(fast, idx);
        int bid = 0;
        const char *text = "";
        unsigned int bflags = 0;
        if(!PyArg_ParseTuple(b, "is|I", &bid, &text, &bflags)) {
            PyMem_Free(buttons);
            Py_DECREF(fast);
            return NULL;
        }
        buttons[idx].buttonid = bid;
        buttons[idx].text = text;
        buttons[idx].flags = bflags;
    }
    data.numbuttons = (int)nbuttons;
    data.buttons = buttons;

    int result = -1;
    int rc = SDL_ShowMessageBox(&data, &result);
    PyMem_Free(buttons);
    Py_DECREF(fast);
    if(0 > rc) {
        return _raise();
    }
    return PyLong_FromLong(result);
}

//=========================================================
// hints
//=========================================================

static PyObject * PySDL_SetHint(PyObject *self, PyObject *args) {
    const char *name;
    const char *value;
    if(!PyArg_ParseTuple(args, "ss", &name, &value)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_SetHint(name, value));
}

static PyObject * PySDL_SetHintWithPriority(PyObject *self, PyObject *args) {
    const char *name;
    const char *value;
    int priority;
    if(!PyArg_ParseTuple(args, "ssi", &name, &value, &priority)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_SetHintWithPriority(name, value, (SDL_HintPriority)priority));
}

static PyObject * PySDL_GetHint(PyObject *self, PyObject *arg) {
    const char *name = PyUnicode_AsUTF8(arg);
    if(NULL == name) {
        return NULL;
    }
    const char *value = SDL_GetHint(name);
    if(NULL == value) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(value);
}

static PyObject * PySDL_GetHintBoolean(PyObject *self, PyObject *args) {
    const char *name;
    int default_value = 0;
    if(!PyArg_ParseTuple(args, "s|p", &name, &default_value)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_GetHintBoolean(name, default_value ? true : false));
}

#if SDL_VERSION_ATLEAST(2,24,0)
static PyObject * PySDL_ResetHint(PyObject *self, PyObject *arg) {
    const char *name = PyUnicode_AsUTF8(arg);
    if(NULL == name) {
        return NULL;
    }
    return PyBool_FromLong(SDL_ResetHint(name));
}
#endif

static PyObject * PySDL_ClearHints(PyObject *self, PyObject *ign) {
    SDL_ClearHints();
    Py_RETURN_NONE;
}

//=========================================================
// misc
//=========================================================

#if SDL_VERSION_ATLEAST(2,0,14)
static PyObject * PySDL_OpenURL(PyObject *self, PyObject *arg) {
    const char *url = PyUnicode_AsUTF8(arg);
    if(NULL == url) {
        return NULL;
    }
    if(0 > SDL_OpenURL(url)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_GetPreferredLocales(PyObject *self, PyObject *ign) {
    SDL_Locale *locales = SDL_GetPreferredLocales();
    if(NULL == locales) {
        return PyList_New(0);
    }
    PyObject *list = PyList_New(0);
    if(NULL == list) {
        SDL_free(locales);
        return NULL;
    }
    for(SDL_Locale *l = locales; l->language != NULL; ++l) {
        PyObject *country = l->country ? PyUnicode_FromString(l->country) : Py_NewRef(Py_None);
        PyObject *entry = country ? Py_BuildValue("(sN)", l->language, country) : NULL;
        if(NULL == entry || 0 > PyList_Append(list, entry)) {
            Py_XDECREF(entry);
            Py_DECREF(list);
            SDL_free(locales);
            return NULL;
        }
        Py_DECREF(entry);
    }
    SDL_free(locales);
    return list;
}
#endif

//=========================================================
// GL / Vulkan loaders
//=========================================================

static PyObject * PySDL_GL_LoadLibrary(PyObject *self, PyObject *args) {
    const char *path = NULL;
    if(!PyArg_ParseTuple(args, "|z", &path)) {
        return NULL;
    }
    if(0 > SDL_GL_LoadLibrary(path)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_GL_UnloadLibrary(PyObject *self, PyObject *ign) {
    SDL_GL_UnloadLibrary();
    Py_RETURN_NONE;
}

static PyObject * PySDL_GL_GetProcAddress(PyObject *self, PyObject *arg) {
    const char *name = PyUnicode_AsUTF8(arg);
    if(NULL == name) {
        return NULL;
    }
    return PyLong_FromVoidPtr(SDL_GL_GetProcAddress(name));
}

static PyObject * PySDL_GL_GetCurrentWindow(PyObject *self, PyObject *ign) {
    return PySDL_WrapWindow(SDL_GL_GetCurrentWindow());
}

static PyObject * PySDL_GL_GetCurrentContext(PyObject *self, PyObject *ign) {
    return PyLong_FromVoidPtr(SDL_GL_GetCurrentContext());
}

static PyObject * PySDL_Vulkan_LoadLibrary(PyObject *self, PyObject *args) {
    const char *path = NULL;
    if(!PyArg_ParseTuple(args, "|z", &path)) {
        return NULL;
    }
    if(0 > SDL_Vulkan_LoadLibrary(path)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Vulkan_UnloadLibrary(PyObject *self, PyObject *ign) {
    SDL_Vulkan_UnloadLibrary();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Vulkan_GetVkGetInstanceProcAddr(PyObject *self, PyObject *ign) {
    void *fn = SDL_Vulkan_GetVkGetInstanceProcAddr();
    if(NULL == fn) {
        return _raise();
    }
    return PyLong_FromVoidPtr(fn);
}

#if SDL_VERSION_ATLEAST(2,0,11)
static PyObject * PySDL_Metal_GetLayer(PyObject *self, PyObject *arg) {
    void *view = PyLong_AsVoidPtr(arg);
    if(NULL == view && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromVoidPtr(SDL_Metal_GetLayer((SDL_MetalView)view));
}

static PyObject * PySDL_Metal_DestroyView(PyObject *self, PyObject *arg) {
    void *view = PyLong_AsVoidPtr(arg);
    if(NULL == view && PyErr_Occurred()) {
        return NULL;
    }
    SDL_Metal_DestroyView((SDL_MetalView)view);
    Py_RETURN_NONE;
}
#endif

static PyObject * PySDL_Vulkan_GetInstanceExtensions(PyObject *self, PyObject *args) {
    PyObject *window_py = Py_None;
    if(!PyArg_ParseTuple(args, "|O", &window_py)) {
        return NULL;
    }

    SDL_Window *window = NULL;
    if(window_py != Py_None) {
        if(!PyObject_TypeCheck(window_py, &PySDL_Window_Type)) {
            PyErr_SetString(PyExc_TypeError, "window must be an SDL2.Window or None");
            return NULL;
        }
        window = ((PySDL_Window *)window_py)->window;
    }

    unsigned int count = 0;
    if(false == SDL_Vulkan_GetInstanceExtensions(window, &count, NULL)) {
        return _raise();
    }

    const char **names = PyMem_New(const char *, count > 0 ? count : 1);
    if(NULL == names) {
        return PyErr_NoMemory();
    }
    if(false == SDL_Vulkan_GetInstanceExtensions(window, &count, names)) {
        PyMem_Free(names);
        return _raise();
    }

    PyObject *list = PyList_New(count);
    if(NULL == list) {
        PyMem_Free(names);
        return NULL;
    }
    for(unsigned int idx = 0; idx < count; ++idx) {
        PyObject *s = PyUnicode_FromString(names[idx]);
        if(NULL == s) {
            Py_DECREF(list);
            PyMem_Free(names);
            return NULL;
        }
        PyList_SET_ITEM(list, idx, s);
    }
    PyMem_Free(names);
    return list;
}

PyMethodDef pysdl_video_methods[] = {
    { "GetDisplayName",           PySDL_GetDisplayName,           METH_O       },
    { "GetDisplayUsableBounds",   PySDL_GetDisplayUsableBounds,   METH_O       },
    { "GetDisplayOrientation",    PySDL_GetDisplayOrientation,    METH_O       },
    { "GetNumDisplayModes",       PySDL_GetNumDisplayModes,       METH_O       },
    { "GetClosestDisplayMode",    PySDL_GetClosestDisplayMode,    METH_VARARGS },

    { "GetWindowFromID",          PySDL_GetWindowFromID,          METH_O       },

    { "ShowSimpleMessageBox",     (PyCFunction)PySDL_ShowSimpleMessageBox, METH_VARARGS | METH_KEYWORDS },
    { "ShowMessageBox",           PySDL_ShowMessageBox,           METH_O       },

    { "SetHint",                  PySDL_SetHint,                  METH_VARARGS },
    { "SetHintWithPriority",      PySDL_SetHintWithPriority,      METH_VARARGS },
    { "GetHint",                  PySDL_GetHint,                  METH_O       },
    { "GetHintBoolean",           PySDL_GetHintBoolean,           METH_VARARGS },
    { "ClearHints",               PySDL_ClearHints,               METH_NOARGS  },
#if SDL_VERSION_ATLEAST(2,0,11)
    { "Metal_GetLayer",           PySDL_Metal_GetLayer,           METH_O       },
    { "Metal_DestroyView",        PySDL_Metal_DestroyView,        METH_O       },
#endif
#if SDL_VERSION_ATLEAST(2,0,14)
    { "OpenURL",                  PySDL_OpenURL,                  METH_O       },
    { "GetPreferredLocales",      PySDL_GetPreferredLocales,      METH_NOARGS  },
#endif
#if SDL_VERSION_ATLEAST(2,0,16)
    { "GetGrabbedWindow",         PySDL_GetGrabbedWindow,         METH_NOARGS  },
#endif
#if SDL_VERSION_ATLEAST(2,24,0)
    { "GetPointDisplayIndex",     PySDL_GetPointDisplayIndex,     METH_O       },
    { "GetRectDisplayIndex",      PySDL_GetRectDisplayIndex,      METH_O       },
    { "ResetHint",                PySDL_ResetHint,                METH_O       },
#endif
    { "GL_LoadLibrary",           PySDL_GL_LoadLibrary,           METH_VARARGS },
    { "GL_UnloadLibrary",         PySDL_GL_UnloadLibrary,         METH_NOARGS  },
    { "GL_GetProcAddress",        PySDL_GL_GetProcAddress,        METH_O       },
    { "GL_GetCurrentWindow",      PySDL_GL_GetCurrentWindow,      METH_NOARGS  },
    { "GL_GetCurrentContext",     PySDL_GL_GetCurrentContext,     METH_NOARGS  },

    { "Vulkan_LoadLibrary",       PySDL_Vulkan_LoadLibrary,       METH_VARARGS },
    { "Vulkan_UnloadLibrary",     PySDL_Vulkan_UnloadLibrary,     METH_NOARGS  },
    { "Vulkan_GetVkGetInstanceProcAddr", PySDL_Vulkan_GetVkGetInstanceProcAddr, METH_NOARGS },
    { "Vulkan_GetInstanceExtensions",    PySDL_Vulkan_GetInstanceExtensions,    METH_VARARGS },

    { NULL }
};
