#include "pysdl.h"

// Module-level video functions: extra display queries, message boxes, hints,
// misc (OpenURL / locales), the GL and Vulkan loader entry points, the
// borrowed-window lookups and CreateWindowAndRenderer. Registered via
// PyModule_AddFunctions.

static PyObject * _raise(void) {
    PyErr_SetString(pysdl_Error, SDL_GetError());
    return NULL;
}

//=========================================================
// displays  (SDL3: identified by SDL_DisplayID, not an index)
//=========================================================

static PyObject * PySDL_GetDisplayName(PyObject *self, PyObject *arg) {
    unsigned long display = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == display && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_GetDisplayName((SDL_DisplayID)display);
    if(NULL == name) {
        return _raise();
    }
    return PyUnicode_FromString(name);
}

static PyObject * PySDL_GetDisplayUsableBounds(PyObject *self, PyObject *arg) {
    unsigned long display = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == display && PyErr_Occurred()) {
        return NULL;
    }
    SDL_Rect rect;
    if(!SDL_GetDisplayUsableBounds((SDL_DisplayID)display, &rect)) {
        return _raise();
    }
    return Py_BuildValue("(iiii)", rect.x, rect.y, rect.w, rect.h);
}

static PyObject * PySDL_GetDisplayOrientation(PyObject *self, PyObject *arg) {
    unsigned long display = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == display && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetCurrentDisplayOrientation((SDL_DisplayID)display));
}

static PyObject * PySDL_GetNumDisplayModes(PyObject *self, PyObject *arg) {
    unsigned long display = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == display && PyErr_Occurred()) {
        return NULL;
    }
    int count = 0;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes((SDL_DisplayID)display, &count);
    if(NULL == modes) {
        return _raise();
    }
    SDL_free(modes);
    return PyLong_FromLong(count);
}

static PyObject * PySDL_GetClosestDisplayMode(PyObject *self, PyObject *args) {
    unsigned long display;
    int fmt = 0, w = 0, h = 0;
    float rate = 0.0f;
    if(!PyArg_ParseTuple(args, "k(iiif)", &display, &fmt, &w, &h, &rate)) {
        return NULL;
    }
    SDL_DisplayMode closest;
    if(!SDL_GetClosestFullscreenDisplayMode((SDL_DisplayID)display, w, h, rate, false, &closest)) {
        Py_RETURN_NONE;
    }
    return Py_BuildValue("(iiif)", closest.format, closest.w, closest.h, closest.refresh_rate);
}

static PyObject * PySDL_GetPointDisplayIndex(PyObject *self, PyObject *arg) {
    SDL_Point point;
    if(!PyToPoint(arg, &point)) {
        return NULL;
    }
    SDL_DisplayID id = SDL_GetDisplayForPoint(&point);
    if(0 == id) {
        return _raise();
    }
    return PyLong_FromUnsignedLong(id);
}

static PyObject * PySDL_GetRectDisplayIndex(PyObject *self, PyObject *arg) {
    SDL_Rect rect;
    if(!PyToRect(arg, &rect)) {
        return NULL;
    }
    SDL_DisplayID id = SDL_GetDisplayForRect(&rect);
    if(0 == id) {
        return _raise();
    }
    return PyLong_FromUnsignedLong(id);
}

//=========================================================
// borrowed-window lookups
//=========================================================

static PyObject * PySDL_GetWindowFromID(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    return PySDL_WrapWindow(SDL_GetWindowFromID((SDL_WindowID)id));
}

static PyObject * PySDL_GetGrabbedWindow(PyObject *self, PyObject *ign) {
    return PySDL_WrapWindow(SDL_GetGrabbedWindow());
}

// CreateWindowAndRenderer(title, size, flags=0) -> (Window, Renderer), both
// owned. The wrappers are made first so a failure leaves nothing to undo.
static PyObject * PySDL_CreateWindowAndRenderer(PyObject *self, PyObject *args, PyObject *kwds) {
    const char *title;
    int w, h;
    unsigned long long flags = 0;
    static char *kwlist[] = {"title", "size", "flags", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "s(ii)|K", kwlist, &title, &w, &h, &flags)) {
        return NULL;
    }

    PySDL_Window *window = (PySDL_Window *)PySDL_New(&PySDL_Window_Type);
    if(NULL == window) {
        return NULL;
    }
    PySDL_Renderer *renderer = (PySDL_Renderer *)PySDL_New(&PySDL_Renderer_Type);
    if(NULL == renderer) {
        Py_DECREF(window);
        return NULL;
    }
    // On failure SDL destroys what it made and leaves both pointers NULL.
    if(!SDL_CreateWindowAndRenderer(title, w, h, (SDL_WindowFlags)flags, &window->window, &renderer->renderer)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        Py_DECREF(renderer);
        Py_DECREF(window);
        return NULL;
    }
    return Py_BuildValue("(NN)", (PyObject *)window, (PyObject *)renderer);
}

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
            PyErr_SetString(PyExc_TypeError, "window must be an SDL3.Window or None");
            return NULL;
        }
        window = ((PySDL_Window *)window_py)->window;
    }

    if(!SDL_ShowSimpleMessageBox(flags, title, message, window)) {
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
    data.flags = v ? (SDL_MessageBoxFlags)PyLong_AsUnsignedLong(v) : SDL_MESSAGEBOX_INFORMATION;
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
            PyErr_SetString(PyExc_TypeError, "window must be an SDL3.Window or None");
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
        buttons[idx].buttonID = bid;
        buttons[idx].text = text;
        buttons[idx].flags = (SDL_MessageBoxButtonFlags)bflags;
    }
    data.numbuttons = (int)nbuttons;
    data.buttons = buttons;

    int result = -1;
    bool ok = SDL_ShowMessageBox(&data, &result);
    PyMem_Free(buttons);
    Py_DECREF(fast);
    if(!ok) {
        return _raise();
    }
    return PyLong_FromLong(result);
}

//=========================================================
// hints
//=========================================================

// SDL runs hint callbacks with its hint lock held, and ours then take the GIL,
// so every call that sets, resets or reads a hint (or adds a callback) drops
// the GIL around SDL - the same rule as the event queue (pysdl_Events.c).

static PyObject * PySDL_SetHint(PyObject *self, PyObject *args) {
    const char *name;
    const char *value;
    if(!PyArg_ParseTuple(args, "ss", &name, &value)) {
        return NULL;
    }
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_SetHint(name, value);
    Py_END_ALLOW_THREADS
    return PyBool_FromLong(ok);
}

static PyObject * PySDL_SetHintWithPriority(PyObject *self, PyObject *args) {
    const char *name;
    const char *value;
    int priority;
    if(!PyArg_ParseTuple(args, "ssi", &name, &value, &priority)) {
        return NULL;
    }
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_SetHintWithPriority(name, value, (SDL_HintPriority)priority);
    Py_END_ALLOW_THREADS
    return PyBool_FromLong(ok);
}

// The hint's value as a new str, or None. SDL_GetHint's string stays valid
// on this thread (SDL keeps a per-thread copy), so it is read after the GIL
// is back.
static PyObject * _hint_value(const char *name) {
    const char *value;
    Py_BEGIN_ALLOW_THREADS
        value = SDL_GetHint(name);
    Py_END_ALLOW_THREADS
    if(NULL == value) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(value);
}

static PyObject * PySDL_GetHint(PyObject *self, PyObject *arg) {
    const char *name = PyUnicode_AsUTF8(arg);
    if(NULL == name) {
        return NULL;
    }
    return _hint_value(name);
}

static PyObject * PySDL_GetHintBoolean(PyObject *self, PyObject *args) {
    const char *name;
    int default_value = 0;
    if(!PyArg_ParseTuple(args, "s|p", &name, &default_value)) {
        return NULL;
    }
    bool value;
    Py_BEGIN_ALLOW_THREADS
        value = SDL_GetHintBoolean(name, default_value ? true : false);
    Py_END_ALLOW_THREADS
    return PyBool_FromLong(value);
}

// Back to the default (environment) value; callbacks stay and fire.
static PyObject * PySDL_ResetHint(PyObject *self, PyObject *arg) {
    const char *name = PyUnicode_AsUTF8(arg);
    if(NULL == name) {
        return NULL;
    }
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_ResetHint(name);
    Py_END_ALLOW_THREADS
    return PyBool_FromLong(ok);
}

// SDL3 has no SDL_ClearHints: ClearHints is ResetHints, every hint back to its
// default; unlike SDL2's ClearHints it keeps the callbacks (SDL_Quit drops them).
static PyObject * PySDL_ResetHints(PyObject *self, PyObject *ign) {
    Py_BEGIN_ALLOW_THREADS
        SDL_ResetHints();
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

//---------------------------------------------------------
// hint callbacks
//---------------------------------------------------------

// SDL is never handed a Python object. Each watched hint name gets exactly
// one SDL callback - _c_hint_callback, userdata NULL - added with the first
// Python callback for that name and then left in place: SDL walks a hint's
// callback list saving only the next node, so removing a node other than the
// one running (one callback dropping another) would free the node SDL visits
// next. The Python callables live in _hint_callbacks (name -> list, in
// registration order; a key means SDL has our callback), read under the GIL.
// SDL_Quit frees every hint callback: PySDL_HintCallbacksGone empties it.
static PyObject *_hint_callbacks = NULL;

static Py_ssize_t _hint_index(PyObject *list, PyObject *callable) {
    for(Py_ssize_t idx = 0; idx < PyList_GET_SIZE(list); ++idx) {
        if(PyList_GET_ITEM(list, idx) == callable) {
            return idx;
        }
    }
    return -1;
}

// SDL calls this with its hint lock held, on the thread that changed the hint
// (possibly from inside another SDL call), so a pending exception is set aside.
static void SDLCALL _c_hint_callback(void *userdata, const char *name, const char *old, const char *new_) {
    (void)userdata;
    PyGILState_STATE gil;
    if(!PySDL_ThreadEnter(&gil)) {
        return;
    }
    PyObject *etype, *evalue, *etb;
    PyErr_Fetch(&etype, &evalue, &etb);

    PyObject *list = _hint_callbacks ? PyDict_GetItemString(_hint_callbacks, name) : NULL;
    // A snapshot, since a callback may add or remove callbacks; each is still
    // checked for membership, so one removed by an earlier one is skipped.
    PyObject *snapshot = list ? PyList_GetSlice(list, 0, PyList_GET_SIZE(list)) : NULL;
    for(Py_ssize_t idx = 0; snapshot && idx < PyList_GET_SIZE(snapshot); ++idx) {
        PyObject *callable = PyList_GET_ITEM(snapshot, idx);
        list = PyDict_GetItemString(_hint_callbacks, name);
        if(NULL == list || 0 > _hint_index(list, callable)) {
            continue;
        }
        PyObject *result = PyObject_CallFunction(callable, "szz", name, old, new_);
        if(NULL == result) {
            PyErr_Print();
        }
        Py_XDECREF(result);
    }
    Py_XDECREF(snapshot);
    if(PyErr_Occurred()) {
        PyErr_Print();
    }

    PyErr_Restore(etype, evalue, etb);
    PySDL_ThreadLeave(gil);
}

// AddHintCallback(name, callback): callback(name, old, new) runs now with the
// current value (old == new), then on every change of that hint (None when
// unset), on the thread that changed it. Adding the same callback again only
// repeats that first call, as in SDL.
static PyObject * PySDL_AddHintCallback(PyObject *self, PyObject *args) {
    const char *name;
    PyObject *callable;
    if(!PyArg_ParseTuple(args, "sO", &name, &callable)) {
        return NULL;
    }
    if(!PyCallable_Check(callable)) {
        PyErr_SetString(PyExc_TypeError, "callback must be callable");
        return NULL;
    }
    if(NULL == _hint_callbacks && NULL == (_hint_callbacks = PyDict_New())) {
        return NULL;
    }

    PyObject *list = PyDict_GetItemString(_hint_callbacks, name);
    if(NULL != list) {
        // SDL already calls us for this hint: register and make the first call here.
        if(0 > _hint_index(list, callable) && 0 > PyList_Append(list, callable)) {
            return NULL;
        }
        PyObject *value = _hint_value(name);
        if(NULL == value) {
            return NULL;
        }
        PyObject *result = PyObject_CallFunction(callable, "sOO", name, value, value);
        Py_DECREF(value);
        if(NULL == result) {
            PyErr_Print();  // as when SDL makes that call
        }
        Py_XDECREF(result);
        Py_RETURN_NONE;
    }

    list = PyList_New(0);
    if(NULL == list || 0 > PyList_Append(list, callable) || 0 > PyDict_SetItemString(_hint_callbacks, name, list)) {
        Py_XDECREF(list);
        return NULL;
    }
    Py_DECREF(list);  // the dict holds it

    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_AddHintCallback(name, _c_hint_callback, NULL);  // calls it once right away
    Py_END_ALLOW_THREADS
    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        PyObject *etype, *evalue, *etb;
        PyErr_Fetch(&etype, &evalue, &etb);
        if(0 > PyDict_DelItemString(_hint_callbacks, name)) {
            PyErr_Clear();
        }
        PyErr_Restore(etype, evalue, etb);
        return NULL;
    }
    Py_RETURN_NONE;
}

// RemoveHintCallback(name, callback) / DelHintCallback: ValueError if that
// callback is not registered for that hint.
static PyObject * PySDL_RemoveHintCallback(PyObject *self, PyObject *args) {
    const char *name;
    PyObject *callable;
    if(!PyArg_ParseTuple(args, "sO", &name, &callable)) {
        return NULL;
    }
    PyObject *list = _hint_callbacks ? PyDict_GetItemString(_hint_callbacks, name) : NULL;
    Py_ssize_t idx = list ? _hint_index(list, callable) : -1;
    if(0 > idx) {
        PyErr_Format(PyExc_ValueError, "callback is not registered for hint %s", name);
        return NULL;
    }
    if(0 > PySequence_DelItem(list, idx)) {
        return NULL;
    }
    Py_RETURN_NONE;
}

void PySDL_HintCallbacksGone(void) {
    if(NULL != _hint_callbacks) {
        PyDict_Clear(_hint_callbacks);
    }
}

//=========================================================
// misc
//=========================================================

static PyObject * PySDL_OpenURL(PyObject *self, PyObject *arg) {
    const char *url = PyUnicode_AsUTF8(arg);
    if(NULL == url) {
        return NULL;
    }
    if(!SDL_OpenURL(url)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_GetPreferredLocales(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_Locale **locales = SDL_GetPreferredLocales(&count);
    PyObject *list = PyList_New(0);
    if(NULL == list) {
        SDL_free(locales);
        return NULL;
    }
    for(int i = 0; locales && i < count && locales[i]; ++i) {
        SDL_Locale *l = locales[i];
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

//=========================================================
// GL / Vulkan loaders
//=========================================================

static PyObject * PySDL_GL_LoadLibrary(PyObject *self, PyObject *args) {
    const char *path = NULL;
    if(!PyArg_ParseTuple(args, "|z", &path)) {
        return NULL;
    }
    if(!SDL_GL_LoadLibrary(path)) {
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
    return PyLong_FromVoidPtr((void *)(uintptr_t)SDL_GL_GetProcAddress(name));
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
    if(!SDL_Vulkan_LoadLibrary(path)) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Vulkan_UnloadLibrary(PyObject *self, PyObject *ign) {
    SDL_Vulkan_UnloadLibrary();
    Py_RETURN_NONE;
}

static PyObject * PySDL_Vulkan_GetVkGetInstanceProcAddr(PyObject *self, PyObject *ign) {
    SDL_FunctionPointer fn = SDL_Vulkan_GetVkGetInstanceProcAddr();
    if(NULL == fn) {
        return _raise();
    }
    return PyLong_FromVoidPtr((void *)(uintptr_t)fn);
}

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

static PyObject * PySDL_Vulkan_GetInstanceExtensions(PyObject *self, PyObject *args) {
    PyObject *window_py = Py_None;  // SDL3 no longer needs the window; accepted and ignored
    if(!PyArg_ParseTuple(args, "|O", &window_py)) {
        return NULL;
    }

    Uint32 count = 0;
    const char * const *names = SDL_Vulkan_GetInstanceExtensions(&count);
    if(NULL == names) {
        return _raise();
    }

    PyObject *list = PyList_New(count);
    if(NULL == list) {
        return NULL;
    }
    for(Uint32 idx = 0; idx < count; ++idx) {
        PyObject *s = PyUnicode_FromString(names[idx]);
        if(NULL == s) {
            Py_DECREF(list);
            return NULL;
        }
        PyList_SET_ITEM(list, idx, s);
    }
    return list;
}

PyMethodDef pysdl_video_methods[] = {
    { "GetDisplayName",           PySDL_GetDisplayName,           METH_O       },
    { "GetDisplayUsableBounds",   PySDL_GetDisplayUsableBounds,   METH_O       },
    { "GetDisplayOrientation",    PySDL_GetDisplayOrientation,    METH_O       },
    { "GetNumDisplayModes",       PySDL_GetNumDisplayModes,       METH_O       },
    { "GetClosestDisplayMode",    PySDL_GetClosestDisplayMode,    METH_VARARGS },
    { "GetPointDisplayIndex",     PySDL_GetPointDisplayIndex,     METH_O       },
    { "GetRectDisplayIndex",      PySDL_GetRectDisplayIndex,      METH_O       },

    { "GetWindowFromID",          PySDL_GetWindowFromID,          METH_O       },
    { "GetGrabbedWindow",         PySDL_GetGrabbedWindow,         METH_NOARGS  },
    { "CreateWindowAndRenderer",  (PyCFunction)PySDL_CreateWindowAndRenderer, METH_VARARGS | METH_KEYWORDS },

    { "ShowSimpleMessageBox",     (PyCFunction)PySDL_ShowSimpleMessageBox, METH_VARARGS | METH_KEYWORDS },
    { "ShowMessageBox",           PySDL_ShowMessageBox,           METH_O       },

    { "SetHint",                  PySDL_SetHint,                  METH_VARARGS },
    { "SetHintWithPriority",      PySDL_SetHintWithPriority,      METH_VARARGS },
    { "GetHint",                  PySDL_GetHint,                  METH_O       },
    { "GetHintBoolean",           PySDL_GetHintBoolean,           METH_VARARGS },
    { "ResetHint",                PySDL_ResetHint,                METH_O       },
    { "ResetHints",               PySDL_ResetHints,               METH_NOARGS  },
    { "ClearHints",               PySDL_ResetHints,               METH_NOARGS  },
    { "AddHintCallback",          PySDL_AddHintCallback,          METH_VARARGS },
    { "RemoveHintCallback",       PySDL_RemoveHintCallback,       METH_VARARGS },
    { "DelHintCallback",          PySDL_RemoveHintCallback,       METH_VARARGS },

    { "OpenURL",                  PySDL_OpenURL,                  METH_O       },
    { "GetPreferredLocales",      PySDL_GetPreferredLocales,      METH_NOARGS  },

    { "Metal_GetLayer",           PySDL_Metal_GetLayer,           METH_O       },
    { "Metal_DestroyView",        PySDL_Metal_DestroyView,        METH_O       },

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
