#include "pysdl.h"

static int        PySDL_Window_Type_init    (PySDL_Window*, PyObject*, PyObject*);
static void       PySDL_Window_Type_dealloc (PySDL_Window* );

static PyObject * PySDL_Window_GetWindowID         (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowTitle      (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowPosition   (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowSize       (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowSurface    (PySDL_Window*, PyObject*);

static PyObject * PySDL_Window_SetWindowFullscreen (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowIcon       (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowTitle      (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowPosition   (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowSize       (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowResizable  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowBrightness (PySDL_Window*, PyObject*);

static PyObject * PySDL_Window_ShowWindow          (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_HideWindow          (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_RaiseWindow         (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_MaximizeWindow      (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_MinimizeWindow      (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_RestoreWindow       (PySDL_Window*, PyObject*);

static PyObject * PySDL_Window_UpdateWindowSurface (PySDL_Window*, PyObject*);

static PyObject * PySDL_Window_GetWindowFlags        (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowDisplayIndex (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowPixelFormat  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowMinimumSize  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowMinimumSize  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowMaximumSize  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowMaximumSize  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowBordered     (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowInputFocus   (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowModalFor     (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowGrab         (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowGrab         (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowOpacity      (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowOpacity      (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowBordersSize  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_UpdateWindowSurfaceRects (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowGammaRamp    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowGammaRamp    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowDisplayMode  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowDisplayMode  (PySDL_Window*, PyObject*);
#if SDL_VERSION_ATLEAST(2,0,16)
static PyObject * PySDL_Window_SetWindowAlwaysOnTop  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowKeyboardGrab (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowKeyboardGrab (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowMouseGrab    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowMouseGrab    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_FlashWindow           (PySDL_Window*, PyObject*);
#endif
#if SDL_VERSION_ATLEAST(2,0,18)
static PyObject * PySDL_Window_SetWindowMouseRect    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowMouseRect    (PySDL_Window*, PyObject*);
#endif

static PyObject * PySDL_Window_CreateRenderer      (PySDL_Window*, PyObject*, PyObject*);
static PyObject * PySDL_Window_GL_CreateContext    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GL_DeleteContext    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GL_MakeCurrent      (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GL_SwapWindow       (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GL_GetDrawableSize  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_Vulkan_GetDrawableSize (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_Vulkan_CreateSurface   (PySDL_Window*, PyObject*);
#if SDL_VERSION_ATLEAST(2,0,11)
static PyObject * PySDL_Window_Metal_CreateView      (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_Metal_GetDrawableSize (PySDL_Window*, PyObject*);
#endif

static PyMethodDef PySDL_Window_methods[] = {
    { "GetWindowID",         (PyCFunction)PySDL_Window_GetWindowID,         METH_NOARGS  },
    { "GetWindowTitle",      (PyCFunction)PySDL_Window_GetWindowTitle,      METH_NOARGS  },
    { "GetWindowPosition",   (PyCFunction)PySDL_Window_GetWindowPosition,   METH_NOARGS  },
    { "GetWindowSize",       (PyCFunction)PySDL_Window_GetWindowSize,       METH_NOARGS  },
    { "GetWindowSurface",    (PyCFunction)PySDL_Window_GetWindowSurface,    METH_NOARGS  },

    { "SetWindowFullscreen", (PyCFunction)PySDL_Window_SetWindowFullscreen, METH_VARARGS },
    { "SetWindowIcon",       (PyCFunction)PySDL_Window_SetWindowIcon,       METH_O       },
    { "SetWindowTitle",      (PyCFunction)PySDL_Window_SetWindowTitle,      METH_O       },
    { "SetWindowPosition",   (PyCFunction)PySDL_Window_SetWindowPosition,   METH_VARARGS },
    { "SetWindowSize",       (PyCFunction)PySDL_Window_SetWindowSize,       METH_VARARGS },
    { "SetWindowResizable",  (PyCFunction)PySDL_Window_SetWindowResizable,  METH_O       },
    { "SetWindowBrightness", (PyCFunction)PySDL_Window_SetWindowBrightness, METH_O       },

    { "ShowWindow",          (PyCFunction)PySDL_Window_ShowWindow,          METH_NOARGS  },
    { "HideWindow",          (PyCFunction)PySDL_Window_HideWindow,          METH_NOARGS  },
    { "RaiseWindow",         (PyCFunction)PySDL_Window_RaiseWindow,         METH_NOARGS  },
    { "MaximizeWindow",      (PyCFunction)PySDL_Window_MaximizeWindow,      METH_NOARGS  },
    { "MinimizeWindow",      (PyCFunction)PySDL_Window_MinimizeWindow,      METH_NOARGS  },
    { "RestoreWindow",       (PyCFunction)PySDL_Window_RestoreWindow,       METH_NOARGS  },

    { "UpdateWindowSurface", (PyCFunction)PySDL_Window_UpdateWindowSurface, METH_NOARGS  },

    { "CreateRenderer",      (PyCFunction)PySDL_Window_CreateRenderer,      METH_VARARGS | METH_KEYWORDS },
    { "GL_CreateContext",    (PyCFunction)PySDL_Window_GL_CreateContext,    METH_NOARGS  },
    { "GL_DeleteContext",    (PyCFunction)PySDL_Window_GL_DeleteContext,    METH_NOARGS  },
    { "GL_MakeCurrent",      (PyCFunction)PySDL_Window_GL_MakeCurrent,      METH_NOARGS  },
    { "GL_SwapWindow",       (PyCFunction)PySDL_Window_GL_SwapWindow,       METH_NOARGS  },
    { "GL_GetDrawableSize",  (PyCFunction)PySDL_Window_GL_GetDrawableSize,  METH_NOARGS  },

    { "GetWindowFlags",        (PyCFunction)PySDL_Window_GetWindowFlags,        METH_NOARGS  },
    { "GetWindowDisplayIndex", (PyCFunction)PySDL_Window_GetWindowDisplayIndex, METH_NOARGS  },
    { "GetWindowPixelFormat",  (PyCFunction)PySDL_Window_GetWindowPixelFormat,  METH_NOARGS  },
    { "SetWindowMinimumSize",  (PyCFunction)PySDL_Window_SetWindowMinimumSize,  METH_VARARGS },
    { "GetWindowMinimumSize",  (PyCFunction)PySDL_Window_GetWindowMinimumSize,  METH_NOARGS  },
    { "SetWindowMaximumSize",  (PyCFunction)PySDL_Window_SetWindowMaximumSize,  METH_VARARGS },
    { "GetWindowMaximumSize",  (PyCFunction)PySDL_Window_GetWindowMaximumSize,  METH_NOARGS  },
    { "SetWindowBordered",     (PyCFunction)PySDL_Window_SetWindowBordered,     METH_O       },
    { "SetWindowInputFocus",   (PyCFunction)PySDL_Window_SetWindowInputFocus,   METH_NOARGS  },
    { "SetWindowModalFor",     (PyCFunction)PySDL_Window_SetWindowModalFor,     METH_O       },
    { "SetWindowGrab",         (PyCFunction)PySDL_Window_SetWindowGrab,         METH_O       },
    { "GetWindowGrab",         (PyCFunction)PySDL_Window_GetWindowGrab,         METH_NOARGS  },
    { "SetWindowOpacity",      (PyCFunction)PySDL_Window_SetWindowOpacity,      METH_O       },
    { "GetWindowOpacity",      (PyCFunction)PySDL_Window_GetWindowOpacity,      METH_NOARGS  },
    { "GetWindowBordersSize",  (PyCFunction)PySDL_Window_GetWindowBordersSize,  METH_NOARGS  },
    { "UpdateWindowSurfaceRects", (PyCFunction)PySDL_Window_UpdateWindowSurfaceRects, METH_O },
    { "SetWindowGammaRamp",    (PyCFunction)PySDL_Window_SetWindowGammaRamp,    METH_VARARGS },
    { "GetWindowGammaRamp",    (PyCFunction)PySDL_Window_GetWindowGammaRamp,    METH_NOARGS  },
    { "SetWindowDisplayMode",  (PyCFunction)PySDL_Window_SetWindowDisplayMode,  METH_O       },
    { "GetWindowDisplayMode",  (PyCFunction)PySDL_Window_GetWindowDisplayMode,  METH_NOARGS  },
#if SDL_VERSION_ATLEAST(2,0,16)
    { "SetWindowAlwaysOnTop",  (PyCFunction)PySDL_Window_SetWindowAlwaysOnTop,  METH_O       },
    { "SetWindowKeyboardGrab", (PyCFunction)PySDL_Window_SetWindowKeyboardGrab, METH_O       },
    { "GetWindowKeyboardGrab", (PyCFunction)PySDL_Window_GetWindowKeyboardGrab, METH_NOARGS  },
    { "SetWindowMouseGrab",    (PyCFunction)PySDL_Window_SetWindowMouseGrab,    METH_O       },
    { "GetWindowMouseGrab",    (PyCFunction)PySDL_Window_GetWindowMouseGrab,    METH_NOARGS  },
    { "FlashWindow",           (PyCFunction)PySDL_Window_FlashWindow,           METH_O       },
#endif
#if SDL_VERSION_ATLEAST(2,0,18)
    { "SetWindowMouseRect",    (PyCFunction)PySDL_Window_SetWindowMouseRect,    METH_O       },
    { "GetWindowMouseRect",    (PyCFunction)PySDL_Window_GetWindowMouseRect,    METH_NOARGS  },
#endif

    { "Vulkan_GetDrawableSize", (PyCFunction)PySDL_Window_Vulkan_GetDrawableSize, METH_NOARGS },
    { "Vulkan_CreateSurface",   (PyCFunction)PySDL_Window_Vulkan_CreateSurface,   METH_O      },
#if SDL_VERSION_ATLEAST(2,0,11)
    { "Metal_CreateView",       (PyCFunction)PySDL_Window_Metal_CreateView,       METH_NOARGS },
    { "Metal_GetDrawableSize",  (PyCFunction)PySDL_Window_Metal_GetDrawableSize,  METH_NOARGS },
#endif
    { NULL }
};

PyTypeObject PySDL_Window_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Window",
    .tp_basicsize = sizeof(PySDL_Window),
    .tp_dealloc   = (destructor)PySDL_Window_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Window(title,(width,height),(x,y),flags)",
    .tp_methods   = PySDL_Window_methods,
    .tp_init      = (initproc)PySDL_Window_Type_init,
    .tp_new       = PyType_GenericNew,
};

static int PySDL_Window_Type_init(PySDL_Window *self, PyObject *args, PyObject *kwds) {
    char *title = NULL;
    int w = 0;
    int h = 0;
    int x = SDL_WINDOWPOS_CENTERED;
    int y = SDL_WINDOWPOS_CENTERED;
    int f = 0;

    static char *kwlist[] = {"title", "size", "position", "flags", NULL};

    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|s(ii)(ii)I", kwlist,
        &title, &w, &h, &x, &y, &f)) {
        return -1;
    }

    self->window = NULL;
    self->glContext = NULL;
    self->shouldFree = 1;

    // No title: internal allocation (borrowed-window wrappers fill ->window in).
    if(NULL != title) {
        self->window = SDL_CreateWindow(title, x, y, w, h, f);
        if(NULL == self->window) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }

    return 0;
}

static void PySDL_Window_Type_dealloc(PySDL_Window *self) {
    if(NULL != self->glContext) {
        SDL_GL_DeleteContext(self->glContext);
        self->glContext = NULL;
    }
    if(NULL != self->window) {
        if(self->shouldFree) {
            SDL_DestroyWindow(self->window);
        }
        self->window = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * PySDL_Window_GetWindowID(PySDL_Window *self, PyObject *ign) {
    Uint32 id = SDL_GetWindowID(self->window);
    if(0 == id) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyLong_FromUnsignedLong(id);
}

static PyObject * PySDL_Window_GetWindowTitle(PySDL_Window *self, PyObject *ign) {
    return PyUnicode_FromString(SDL_GetWindowTitle(self->window));
}

static PyObject * PySDL_Window_GetWindowPosition(PySDL_Window *self, PyObject *ign) {
    int x, y;
    SDL_GetWindowPosition(self->window, &x, &y);
    return Py_BuildValue("(ii)", x, y);
}

static PyObject * PySDL_Window_GetWindowSize(PySDL_Window *self, PyObject *ign) {
    int w, h;
    SDL_GetWindowSize(self->window, &w, &h);
    return Py_BuildValue("(ii)", w, h);
}

static PyObject * PySDL_Window_GetWindowSurface(PySDL_Window *self, PyObject *ign) {
    PySDL_Surface *pysdl_Surface;

    pysdl_Surface = (PySDL_Surface *)PySDL_New(&PySDL_Surface_Type);
    if(NULL == pysdl_Surface) {
        return NULL;
    }

    pysdl_Surface->shouldFree = 0;

    pysdl_Surface->surface = SDL_GetWindowSurface(self->window);
    if(NULL == pysdl_Surface->surface) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    return (PyObject *)pysdl_Surface;
}

static PyObject * PySDL_Window_SetWindowFullscreen(PySDL_Window *self, PyObject *args) {
    int flags = 0;
    int ok;

    ok = PyArg_ParseTuple(args, "|i", &flags);

    ok = SDL_SetWindowFullscreen(self->window, flags);
    if(0 > ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowIcon(PySDL_Window *self, PyObject *args) {
    PySDL_Surface *pysdl_Surface = (PySDL_Surface *)args;
    SDL_SetWindowIcon(self->window, pysdl_Surface->surface);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowTitle(PySDL_Window *self, PyObject *args) {
    SDL_SetWindowTitle(self->window, PyUnicode_AsUTF8(args));
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowPosition(PySDL_Window *self, PyObject *args) {
    int x, y;
    int ok;

    ok = PyArg_ParseTuple(args, "(ii)", &x, &y);
    if(0 > ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    SDL_SetWindowPosition(self->window, x, y);

    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowSize(PySDL_Window *self, PyObject *args) {
    int w, h;
    int ok;

    ok = PyArg_ParseTuple(args, "(ii)", &w, &h);
    if(0 > ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    SDL_SetWindowSize(self->window, w, h);

    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowResizable(PySDL_Window *self, PyObject *args) {
    SDL_SetWindowResizable(self->window, args == Py_True);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowBrightness(PySDL_Window *self, PyObject *args) {
    SDL_SetWindowBrightness(self->window, PyFloat_AsDouble(args));
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_ShowWindow(PySDL_Window *self, PyObject *ign) {
    SDL_ShowWindow(self->window);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_HideWindow(PySDL_Window *self, PyObject *ign) {
    SDL_HideWindow(self->window);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_RaiseWindow(PySDL_Window *self, PyObject *ign) {
    SDL_RaiseWindow(self->window);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_MaximizeWindow(PySDL_Window *self, PyObject *ign) {
    SDL_MaximizeWindow(self->window);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_MinimizeWindow(PySDL_Window *self, PyObject *ign) {
    SDL_MinimizeWindow(self->window);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_RestoreWindow(PySDL_Window *self, PyObject *ign) {
    SDL_RestoreWindow(self->window);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_UpdateWindowSurface(PySDL_Window *self, PyObject *ign) {
    SDL_UpdateWindowSurface(self->window);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_CreateRenderer(PySDL_Window *self, PyObject *args, PyObject *kwds) {
    unsigned int flags = 0;
    int index = -1;

    static char *kwlist[] = {"flags", "index", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|Ii", kwlist, &flags, &index)) {
        return NULL;
    }

    PySDL_Renderer *pysdl_Renderer = (PySDL_Renderer *)PySDL_New(&PySDL_Renderer_Type);
    if(NULL == pysdl_Renderer) {
        return NULL;
    }

    pysdl_Renderer->renderer = SDL_CreateRenderer(self->window, index, flags);
    if(NULL == pysdl_Renderer->renderer) {
        Py_DECREF(pysdl_Renderer);
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    return (PyObject *)pysdl_Renderer;
}

static PyObject * PySDL_Window_GL_CreateContext(PySDL_Window *self, PyObject *ign) {
    self->glContext = SDL_GL_CreateContext(self->window);
    if(NULL == self->glContext) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GL_DeleteContext(PySDL_Window *self, PyObject *ign) {
    if(NULL != self->glContext) {
        SDL_GL_DeleteContext(self->glContext);
        self->glContext = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GL_MakeCurrent(PySDL_Window *self, PyObject *ign) {
    if(NULL != self->glContext) {
        SDL_GL_MakeCurrent(self->window, self->glContext);
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GL_SwapWindow(PySDL_Window *self, PyObject *ign) {
    Py_BEGIN_ALLOW_THREADS
        SDL_GL_SwapWindow(self->window);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GL_GetDrawableSize(PySDL_Window *self, PyObject *ign) {
    int w, h;
    SDL_GL_GetDrawableSize(self->window, &w, &h);
    return Py_BuildValue("(ii)", w, h);
}

//=========================================================
// phase 7: window state
//=========================================================

static PyObject * _wraise(void) {
    PyErr_SetString(pysdl_Error, SDL_GetError());
    return NULL;
}

static PyObject * PySDL_Window_GetWindowFlags(PySDL_Window *self, PyObject *ign) {
    return PyLong_FromUnsignedLong(SDL_GetWindowFlags(self->window));
}

static PyObject * PySDL_Window_GetWindowDisplayIndex(PySDL_Window *self, PyObject *ign) {
    int index = SDL_GetWindowDisplayIndex(self->window);
    if(0 > index) {
        return _wraise();
    }
    return PyLong_FromLong(index);
}

static PyObject * PySDL_Window_GetWindowPixelFormat(PySDL_Window *self, PyObject *ign) {
    return PyLong_FromUnsignedLong(SDL_GetWindowPixelFormat(self->window));
}

static PyObject * PySDL_Window_SetWindowMinimumSize(PySDL_Window *self, PyObject *args) {
    int w, h;
    if(!PyArg_ParseTuple(args, "(ii)", &w, &h)) {
        return NULL;
    }
    SDL_SetWindowMinimumSize(self->window, w, h);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowMinimumSize(PySDL_Window *self, PyObject *ign) {
    int w = 0, h = 0;
    SDL_GetWindowMinimumSize(self->window, &w, &h);
    return Py_BuildValue("(ii)", w, h);
}

static PyObject * PySDL_Window_SetWindowMaximumSize(PySDL_Window *self, PyObject *args) {
    int w, h;
    if(!PyArg_ParseTuple(args, "(ii)", &w, &h)) {
        return NULL;
    }
    SDL_SetWindowMaximumSize(self->window, w, h);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowMaximumSize(PySDL_Window *self, PyObject *ign) {
    int w = 0, h = 0;
    SDL_GetWindowMaximumSize(self->window, &w, &h);
    return Py_BuildValue("(ii)", w, h);
}

static PyObject * PySDL_Window_SetWindowBordered(PySDL_Window *self, PyObject *arg) {
    int on = PyObject_IsTrue(arg);
    if(-1 == on) {
        return NULL;
    }
    SDL_SetWindowBordered(self->window, on ? true : false);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowInputFocus(PySDL_Window *self, PyObject *ign) {
    if(0 > SDL_SetWindowInputFocus(self->window)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowModalFor(PySDL_Window *self, PyObject *arg) {
    if(!PyObject_TypeCheck(arg, &PySDL_Window_Type)) {
        PyErr_SetString(PyExc_TypeError, "expected an SDL2.Window (the parent)");
        return NULL;
    }
    if(0 > SDL_SetWindowModalFor(self->window, ((PySDL_Window *)arg)->window)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowGrab(PySDL_Window *self, PyObject *arg) {
    int on = PyObject_IsTrue(arg);
    if(-1 == on) {
        return NULL;
    }
    SDL_SetWindowGrab(self->window, on ? true : false);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowGrab(PySDL_Window *self, PyObject *ign) {
    return PyBool_FromLong(SDL_GetWindowGrab(self->window));
}

static PyObject * PySDL_Window_SetWindowOpacity(PySDL_Window *self, PyObject *arg) {
    double opacity = PyFloat_AsDouble(arg);
    if(-1.0 == opacity && PyErr_Occurred()) {
        return NULL;
    }
    if(0 > SDL_SetWindowOpacity(self->window, (float)opacity)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowOpacity(PySDL_Window *self, PyObject *ign) {
    float opacity = 1.0f;
    if(0 > SDL_GetWindowOpacity(self->window, &opacity)) {
        return _wraise();
    }
    return PyFloat_FromDouble(opacity);
}

static PyObject * PySDL_Window_GetWindowBordersSize(PySDL_Window *self, PyObject *ign) {
    int top = 0, left = 0, bottom = 0, right = 0;
    if(0 > SDL_GetWindowBordersSize(self->window, &top, &left, &bottom, &right)) {
        return _wraise();
    }
    return Py_BuildValue("(iiii)", top, left, bottom, right);
}

static PyObject * PySDL_Window_UpdateWindowSurfaceRects(PySDL_Window *self, PyObject *arg) {
    PyObject *fast = PySequence_Fast(arg, "expected a list of (x, y, w, h) rects");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    SDL_Rect *rects = PyMem_New(SDL_Rect, n > 0 ? n : 1);
    if(NULL == rects) {
        Py_DECREF(fast);
        return PyErr_NoMemory();
    }
    for(Py_ssize_t idx = 0; idx < n; ++idx) {
        if(!PyToRect(PySequence_Fast_GET_ITEM(fast, idx), &rects[idx])) {
            PyMem_Free(rects);
            Py_DECREF(fast);
            return NULL;
        }
    }
    Py_DECREF(fast);
    int rc = SDL_UpdateWindowSurfaceRects(self->window, rects, (int)n);
    PyMem_Free(rects);
    if(0 > rc) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

// gamma ramp: each channel is 256 Uint16 values
static int _ramp_from_py(PyObject *seq, Uint16 *out) {
    PyObject *fast = PySequence_Fast(seq, "each gamma ramp must be 256 values");
    if(NULL == fast) {
        return 0;
    }
    if(256 != PySequence_Fast_GET_SIZE(fast)) {
        PyErr_SetString(PyExc_ValueError, "each gamma ramp must have exactly 256 values");
        Py_DECREF(fast);
        return 0;
    }
    for(int idx = 0; idx < 256; ++idx) {
        long v = PyLong_AsLong(PySequence_Fast_GET_ITEM(fast, idx));
        if(-1 == v && PyErr_Occurred()) {
            Py_DECREF(fast);
            return 0;
        }
        out[idx] = (Uint16)v;
    }
    Py_DECREF(fast);
    return 1;
}

static PyObject * PySDL_Window_SetWindowGammaRamp(PySDL_Window *self, PyObject *args) {
    PyObject *r_py, *g_py, *b_py;
    if(!PyArg_ParseTuple(args, "OOO", &r_py, &g_py, &b_py)) {
        return NULL;
    }
    Uint16 r[256], g[256], b[256];
    if(!_ramp_from_py(r_py, r) || !_ramp_from_py(g_py, g) || !_ramp_from_py(b_py, b)) {
        return NULL;
    }
    if(0 > SDL_SetWindowGammaRamp(self->window, r, g, b)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowGammaRamp(PySDL_Window *self, PyObject *ign) {
    Uint16 r[256], g[256], b[256];
    if(0 > SDL_GetWindowGammaRamp(self->window, r, g, b)) {
        return _wraise();
    }
    PyObject *result = PyTuple_New(3);
    Uint16 *channels[3] = {r, g, b};
    for(int c = 0; c < 3; ++c) {
        PyObject *list = PyList_New(256);
        if(NULL == list) {
            Py_DECREF(result);
            return NULL;
        }
        for(int idx = 0; idx < 256; ++idx) {
            PyList_SET_ITEM(list, idx, PyLong_FromLong(channels[c][idx]));
        }
        PyTuple_SET_ITEM(result, c, list);
    }
    return result;
}

static PyObject * PySDL_Window_SetWindowDisplayMode(PySDL_Window *self, PyObject *arg) {
    SDL_DisplayMode mode;
    SDL_DisplayMode *mp = NULL;
    if(arg != Py_None) {
        SDL_memset(&mode, 0, sizeof(mode));
        if(!PyArg_ParseTuple(arg, "iiii", &mode.format, &mode.w, &mode.h, &mode.refresh_rate)) {
            return NULL;
        }
        mp = &mode;
    }
    if(0 > SDL_SetWindowDisplayMode(self->window, mp)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowDisplayMode(PySDL_Window *self, PyObject *ign) {
    SDL_DisplayMode mode;
    if(0 > SDL_GetWindowDisplayMode(self->window, &mode)) {
        return _wraise();
    }
    return Py_BuildValue("(iiii)", mode.format, mode.w, mode.h, mode.refresh_rate);
}

#if SDL_VERSION_ATLEAST(2,0,16)
static PyObject * PySDL_Window_SetWindowAlwaysOnTop(PySDL_Window *self, PyObject *arg) {
    int on = PyObject_IsTrue(arg);
    if(-1 == on) {
        return NULL;
    }
    SDL_SetWindowAlwaysOnTop(self->window, on ? true : false);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowKeyboardGrab(PySDL_Window *self, PyObject *arg) {
    int on = PyObject_IsTrue(arg);
    if(-1 == on) {
        return NULL;
    }
    SDL_SetWindowKeyboardGrab(self->window, on ? true : false);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowKeyboardGrab(PySDL_Window *self, PyObject *ign) {
    return PyBool_FromLong(SDL_GetWindowKeyboardGrab(self->window));
}

static PyObject * PySDL_Window_SetWindowMouseGrab(PySDL_Window *self, PyObject *arg) {
    int on = PyObject_IsTrue(arg);
    if(-1 == on) {
        return NULL;
    }
    SDL_SetWindowMouseGrab(self->window, on ? true : false);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowMouseGrab(PySDL_Window *self, PyObject *ign) {
    return PyBool_FromLong(SDL_GetWindowMouseGrab(self->window));
}

static PyObject * PySDL_Window_FlashWindow(PySDL_Window *self, PyObject *arg) {
    long op = PyLong_AsLong(arg);
    if(-1 == op && PyErr_Occurred()) {
        return NULL;
    }
    if(0 > SDL_FlashWindow(self->window, (SDL_FlashOperation)op)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}
#endif

#if SDL_VERSION_ATLEAST(2,0,18)
static PyObject * PySDL_Window_SetWindowMouseRect(PySDL_Window *self, PyObject *arg) {
    SDL_Rect rect;
    SDL_Rect *rp = NULL;
    if(arg != Py_None) {
        if(!PyToRect(arg, &rect)) {
            return NULL;
        }
        rp = &rect;
    }
    if(0 > SDL_SetWindowMouseRect(self->window, rp)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowMouseRect(PySDL_Window *self, PyObject *ign) {
    const SDL_Rect *rect = SDL_GetWindowMouseRect(self->window);
    if(NULL == rect) {
        Py_RETURN_NONE;
    }
    return Py_BuildValue("(iiii)", rect->x, rect->y, rect->w, rect->h);
}
#endif

//=========================================================
// vulkan
//=========================================================

static PyObject * PySDL_Window_Vulkan_GetDrawableSize(PySDL_Window *self, PyObject *ign) {
    int w = 0, h = 0;
    SDL_Vulkan_GetDrawableSize(self->window, &w, &h);
    return Py_BuildValue("(ii)", w, h);
}

static PyObject * PySDL_Window_Vulkan_CreateSurface(PySDL_Window *self, PyObject *arg) {
    // `arg` is a VkInstance handle as an int (e.g. from the `vulkan` package).
    unsigned long long instance_handle = PyLong_AsUnsignedLongLong(arg);
    if((unsigned long long)-1 == instance_handle && PyErr_Occurred()) {
        return NULL;
    }
    VkSurfaceKHR surface = 0;
    if(false == SDL_Vulkan_CreateSurface(self->window,
        (VkInstance)(uintptr_t)instance_handle, &surface)) {
        return _wraise();
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)(uintptr_t)surface);
}

#if SDL_VERSION_ATLEAST(2,0,11)
static PyObject * PySDL_Window_Metal_CreateView(PySDL_Window *self, PyObject *ign) {
    // The window must have been created with SDL_WINDOW_METAL. Returns the
    // SDL_MetalView handle as an int; pass it to SDL2.Metal_GetLayer /
    // SDL2.Metal_DestroyView.
    SDL_MetalView view = SDL_Metal_CreateView(self->window);
    if(NULL == view) {
        return _wraise();
    }
    return PyLong_FromVoidPtr(view);
}

static PyObject * PySDL_Window_Metal_GetDrawableSize(PySDL_Window *self, PyObject *ign) {
    int w = 0, h = 0;
    SDL_Metal_GetDrawableSize(self->window, &w, &h);
    return Py_BuildValue("(ii)", w, h);
}
#endif
