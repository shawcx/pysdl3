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
static PyObject * PySDL_Window_SetWindowDisplayMode  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowDisplayMode  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowAlwaysOnTop  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowKeyboardGrab (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowKeyboardGrab (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowMouseGrab    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowMouseGrab    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_FlashWindow           (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowMouseRect    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowMouseRect    (PySDL_Window*, PyObject*);

static PyObject * PySDL_Window_CreateRenderer      (PySDL_Window*, PyObject*, PyObject*);
static PyObject * PySDL_Window_GL_CreateContext    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GL_DeleteContext    (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GL_MakeCurrent      (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GL_SwapWindow       (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetSizeInPixels     (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_Vulkan_CreateSurface   (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_Metal_CreateView       (PySDL_Window*, PyObject*);

static PyObject * PySDL_Window_GetRenderer           (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_GetWindowICCProfile   (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_HasWindowSurface      (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_DestroyWindowSurface  (PySDL_Window*, PyObject*);
static PyObject * PySDL_Window_SetWindowShape        (PySDL_Window*, PyObject*);

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
    { "GL_GetDrawableSize",  (PyCFunction)PySDL_Window_GetSizeInPixels,     METH_NOARGS  },
    { "GetWindowSizeInPixels", (PyCFunction)PySDL_Window_GetSizeInPixels,   METH_NOARGS  },

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
    { "SetWindowDisplayMode",  (PyCFunction)PySDL_Window_SetWindowDisplayMode,  METH_O       },
    { "GetWindowDisplayMode",  (PyCFunction)PySDL_Window_GetWindowDisplayMode,  METH_NOARGS  },
    { "SetWindowAlwaysOnTop",  (PyCFunction)PySDL_Window_SetWindowAlwaysOnTop,  METH_O       },
    { "SetWindowKeyboardGrab", (PyCFunction)PySDL_Window_SetWindowKeyboardGrab, METH_O       },
    { "GetWindowKeyboardGrab", (PyCFunction)PySDL_Window_GetWindowKeyboardGrab, METH_NOARGS  },
    { "SetWindowMouseGrab",    (PyCFunction)PySDL_Window_SetWindowMouseGrab,    METH_O       },
    { "GetWindowMouseGrab",    (PyCFunction)PySDL_Window_GetWindowMouseGrab,    METH_NOARGS  },
    { "FlashWindow",           (PyCFunction)PySDL_Window_FlashWindow,           METH_O       },
    { "SetWindowMouseRect",    (PyCFunction)PySDL_Window_SetWindowMouseRect,    METH_O       },
    { "GetWindowMouseRect",    (PyCFunction)PySDL_Window_GetWindowMouseRect,    METH_NOARGS  },

    { "GetRenderer",           (PyCFunction)PySDL_Window_GetRenderer,           METH_NOARGS  },
    { "GetWindowICCProfile",   (PyCFunction)PySDL_Window_GetWindowICCProfile,   METH_NOARGS  },
    { "HasWindowSurface",      (PyCFunction)PySDL_Window_HasWindowSurface,      METH_NOARGS  },
    { "WindowHasSurface",      (PyCFunction)PySDL_Window_HasWindowSurface,      METH_NOARGS  },
    { "DestroyWindowSurface",  (PyCFunction)PySDL_Window_DestroyWindowSurface,  METH_NOARGS  },
    { "SetWindowShape",        (PyCFunction)PySDL_Window_SetWindowShape,        METH_O       },

    { "Vulkan_GetDrawableSize", (PyCFunction)PySDL_Window_GetSizeInPixels,      METH_NOARGS },
    { "Vulkan_CreateSurface",   (PyCFunction)PySDL_Window_Vulkan_CreateSurface, METH_O      },
    { "Metal_CreateView",       (PyCFunction)PySDL_Window_Metal_CreateView,     METH_NOARGS },
    { "Metal_GetDrawableSize",  (PyCFunction)PySDL_Window_GetSizeInPixels,      METH_NOARGS },
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

static PyObject * _wraise(void) {
    PyErr_SetString(pysdl_Error, SDL_GetError());
    return NULL;
}

//=========================================================
// window / window-surface tracking
//=========================================================

// Every live Window wrapper (owned or borrowed) and every Surface handed out by
// GetWindowSurface. Surfaces are matched by window id rather than pointer, so
// all wrappers of one window share its Surface and nothing here ever touches
// an SDL_Window that may already be gone.
static PySDL_Registry _windows;
static PySDL_Registry _window_surfaces;

static PySDL_Surface * _find_surface(SDL_WindowID id) {
    for(Py_ssize_t idx = 0; idx < _window_surfaces.len; ++idx) {
        PySDL_Surface *surface = (PySDL_Surface *)_window_surfaces.items[idx];
        if(surface->window_id == id) {
            return surface;
        }
    }
    return NULL;
}

// SDL has freed, or is about to free, window `id`'s surface: empty ours.
static void _drop_surface(SDL_WindowID id) {
    PySDL_Surface *surface = id ? _find_surface(id) : NULL;
    if(NULL != surface) {
        PySDL_RegistryRemove(&_window_surfaces, (PyObject *)surface);
        PySDL_SurfaceDetach(surface);
    }
}

// Is `window` `root` or one of its descendants? SDL_DestroyWindow takes the
// child windows (SetWindowModalFor parents them) down with the parent.
static int _within(SDL_Window *window, SDL_Window *root) {
    for(; NULL != window; window = SDL_GetWindowParent(window)) {
        if(window == root) {
            return 1;
        }
    }
    return 0;
}

// The owner is about to SDL_DestroyWindow(root), which also destroys its
// child windows and, with each window, its surface and its renderer (and so
// the renderer's textures). Destroy the renderers first (SDL would leave a
// husk some SDL calls don't check), empty the surfaces and null every other
// wrapper of those windows (borrowed ones from GetWindowFromID,
// GetKeyboardFocus, ..., and a child's own).
static void _destroying(SDL_Window *root, PySDL_Window *owner) {
    for(Py_ssize_t idx = 0; idx < _windows.len; ++idx) {
        PySDL_Window *other = (PySDL_Window *)_windows.items[idx];
        SDL_Window *window = other->window;
        if(NULL != window && window != root && _within(window, root)) {
            PySDL_RendererWindowGone(window);
            _drop_surface(SDL_GetWindowID(window));
        }
    }
    PySDL_RendererWindowGone(root);
    _drop_surface(SDL_GetWindowID(root));
    for(Py_ssize_t idx = 0; idx < _windows.len; ++idx) {
        PySDL_Window *other = (PySDL_Window *)_windows.items[idx];
        if(other != owner && NULL != other->window && _within(other->window, root)) {
            other->window = NULL;
        }
    }
}

void PySDL_WindowSurfaceForget(PySDL_Surface *surface) {
    PySDL_RegistryRemove(&_window_surfaces, (PyObject *)surface);
    surface->window_id = 0;
}

void PySDL_WindowSurfaceDrop(SDL_Window *window) {
    _drop_surface(SDL_GetWindowID(window));
}

// Video shut down (SDL_Quit, QuitSubSystem(INIT_VIDEO)): SDL destroyed every
// window, window surface, renderer and texture. Null the Windows (their
// methods then raise "Invalid window" and dealloc does nothing), the Renderers
// and Textures, and empty the Surfaces.
void PySDL_InvalidateWindows(void) {
    PySDL_InvalidateRenderers();
    for(Py_ssize_t idx = 0; idx < _windows.len; ++idx) {
        PySDL_Window *window = (PySDL_Window *)_windows.items[idx];
        window->window = NULL;
        window->glContext = NULL;  // died with the video subsystem
    }
    while(_window_surfaces.len > 0) {
        PySDL_Surface *surface = (PySDL_Surface *)_window_surfaces.items[_window_surfaces.len - 1];
        _window_surfaces.len--;
        PySDL_SurfaceDetach(surface);
    }
}

void PySDL_CheckVideoGone(void) {
    if(NULL == SDL_GetCurrentVideoDriver()) {
        PySDL_InvalidateWindows();
    }
}

static int PySDL_Window_Type_init(PySDL_Window *self, PyObject *args, PyObject *kwds) {
    char *title = NULL;
    int w = 0;
    int h = 0;
    int x = SDL_WINDOWPOS_CENTERED;
    int y = SDL_WINDOWPOS_CENTERED;
    unsigned long long f = 0;

    static char *kwlist[] = {"title", "size", "position", "flags", NULL};

    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|s(ii)(ii)K", kwlist,
        &title, &w, &h, &x, &y, &f)) {
        return -1;
    }

    self->window = NULL;
    self->glContext = NULL;
    self->shouldFree = 1;
    if(0 > PySDL_RegistryAdd(&_windows, (PyObject *)self)) {
        return -1;
    }

    // No title: internal allocation (borrowed-window wrappers fill ->window in).
    if(NULL != title) {
        // SDL3 dropped the x/y args from SDL_CreateWindow; position afterwards.
        self->window = SDL_CreateWindow(title, w, h, (SDL_WindowFlags)f);
        if(NULL == self->window) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
        SDL_SetWindowPosition(self->window, x, y);
    }

    return 0;
}

static void PySDL_Window_Type_dealloc(PySDL_Window *self) {
    PySDL_RegistryRemove(&_windows, (PyObject *)self);
    if(NULL != self->glContext) {
        SDL_GL_DestroyContext(self->glContext);
        self->glContext = NULL;
    }
    if(NULL != self->window) {
        if(self->shouldFree) {
            _destroying(self->window, self);
            SDL_DestroyWindow(self->window);
        }
        self->window = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject * PySDL_Window_GetWindowID(PySDL_Window *self, PyObject *ign) {
    Uint32 id = SDL_GetWindowID(self->window);
    if(0 == id) {
        return _wraise();
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

// The window's surface. While SDL keeps the same surface this returns the same
// Surface object; when SDL replaces it (after a resize) the previous Surface
// is emptied (0x0) instead of left pointing at freed memory.
static PyObject * PySDL_Window_GetWindowSurface(PySDL_Window *self, PyObject *ign) {
    SDL_WindowID id = self->window ? SDL_GetWindowID(self->window) : 0;
    SDL_Surface *surface = SDL_GetWindowSurface(self->window);

    PySDL_Surface *cached = id ? _find_surface(id) : NULL;
    if(NULL != cached && NULL != surface && cached->surface == surface) {
        return Py_NewRef((PyObject *)cached);
    }
    _drop_surface(id);  // replaced, or freed before SDL failed to make a new one
    if(NULL == surface) {
        return _wraise();
    }

    PySDL_Surface *wrapper = (PySDL_Surface *)PySDL_New(&PySDL_Surface_Type);
    if(NULL == wrapper) {
        return NULL;
    }
    wrapper->surface = surface;
    wrapper->shouldFree = 0;  // SDL owns the window surface
    wrapper->window_id = id;
    if(0 > PySDL_RegistryAdd(&_window_surfaces, (PyObject *)wrapper)) {
        wrapper->window_id = 0;  // untracked: don't let dealloc look for it
        Py_DECREF(wrapper);
        return NULL;
    }
    return (PyObject *)wrapper;
}

static PyObject * PySDL_Window_SetWindowFullscreen(PySDL_Window *self, PyObject *args) {
    int enable = 1;
    if(!PyArg_ParseTuple(args, "|p", &enable)) {
        return NULL;
    }
    if(!SDL_SetWindowFullscreen(self->window, enable ? true : false)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowIcon(PySDL_Window *self, PyObject *args) {
    if(!PyObject_TypeCheck(args, &PySDL_Surface_Type)) {
        PyErr_SetString(PyExc_TypeError, "expected an SDL3.Surface");
        return NULL;
    }
    SDL_SetWindowIcon(self->window, ((PySDL_Surface *)args)->surface);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowTitle(PySDL_Window *self, PyObject *args) {
    const char *title = PyUnicode_AsUTF8(args);
    if(NULL == title) {
        return NULL;
    }
    SDL_SetWindowTitle(self->window, title);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowPosition(PySDL_Window *self, PyObject *args) {
    int x, y;
    if(!PyArg_ParseTuple(args, "(ii)", &x, &y)) {
        return NULL;
    }
    SDL_SetWindowPosition(self->window, x, y);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowSize(PySDL_Window *self, PyObject *args) {
    int w, h;
    if(!PyArg_ParseTuple(args, "(ii)", &w, &h)) {
        return NULL;
    }
    SDL_SetWindowSize(self->window, w, h);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowResizable(PySDL_Window *self, PyObject *args) {
    SDL_SetWindowResizable(self->window, PyObject_IsTrue(args) ? true : false);
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
    const char *name = NULL;

    static char *kwlist[] = {"name", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|z", kwlist, &name)) {
        return NULL;
    }

    PySDL_Renderer *pysdl_Renderer = (PySDL_Renderer *)PySDL_New(&PySDL_Renderer_Type);
    if(NULL == pysdl_Renderer) {
        return NULL;
    }

    pysdl_Renderer->renderer = SDL_CreateRenderer(self->window, name);
    if(NULL == pysdl_Renderer->renderer) {
        Py_DECREF(pysdl_Renderer);
        return _wraise();
    }

    return (PyObject *)pysdl_Renderer;
}

static PyObject * PySDL_Window_GL_CreateContext(PySDL_Window *self, PyObject *ign) {
    self->glContext = SDL_GL_CreateContext(self->window);
    if(NULL == self->glContext) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GL_DeleteContext(PySDL_Window *self, PyObject *ign) {
    if(NULL != self->glContext) {
        SDL_GL_DestroyContext(self->glContext);
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

static PyObject * PySDL_Window_GetSizeInPixels(PySDL_Window *self, PyObject *ign) {
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(self->window, &w, &h);
    return Py_BuildValue("(ii)", w, h);
}

//=========================================================
// window state
//=========================================================

static PyObject * PySDL_Window_GetWindowFlags(PySDL_Window *self, PyObject *ign) {
    return PyLong_FromUnsignedLongLong(SDL_GetWindowFlags(self->window));
}

static PyObject * PySDL_Window_GetWindowDisplayIndex(PySDL_Window *self, PyObject *ign) {
    SDL_DisplayID id = SDL_GetDisplayForWindow(self->window);
    if(0 == id) {
        return _wraise();
    }
    return PyLong_FromUnsignedLong(id);
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
    if(!SDL_SetWindowFocusable(self->window, true) || !SDL_RaiseWindow(self->window)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowModalFor(PySDL_Window *self, PyObject *arg) {
    SDL_Window *parent = NULL;
    if(arg != Py_None) {
        if(!PyObject_TypeCheck(arg, &PySDL_Window_Type)) {
            PyErr_SetString(PyExc_TypeError, "expected an SDL3.Window (the parent) or None");
            return NULL;
        }
        parent = ((PySDL_Window *)arg)->window;
    }
    if(!SDL_SetWindowParent(self->window, parent)
        || !SDL_SetWindowModal(self->window, parent != NULL)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowGrab(PySDL_Window *self, PyObject *arg) {
    int on = PyObject_IsTrue(arg);
    if(-1 == on) {
        return NULL;
    }
    SDL_SetWindowMouseGrab(self->window, on ? true : false);
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowGrab(PySDL_Window *self, PyObject *ign) {
    return PyBool_FromLong(SDL_GetWindowMouseGrab(self->window));
}

static PyObject * PySDL_Window_SetWindowOpacity(PySDL_Window *self, PyObject *arg) {
    double opacity = PyFloat_AsDouble(arg);
    if(-1.0 == opacity && PyErr_Occurred()) {
        return NULL;
    }
    if(!SDL_SetWindowOpacity(self->window, (float)opacity)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowOpacity(PySDL_Window *self, PyObject *ign) {
    float opacity = SDL_GetWindowOpacity(self->window);
    if(opacity < 0.0f) {
        return _wraise();
    }
    return PyFloat_FromDouble(opacity);
}

static PyObject * PySDL_Window_GetWindowBordersSize(PySDL_Window *self, PyObject *ign) {
    int top = 0, left = 0, bottom = 0, right = 0;
    if(!SDL_GetWindowBordersSize(self->window, &top, &left, &bottom, &right)) {
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
    bool ok = SDL_UpdateWindowSurfaceRects(self->window, rects, (int)n);
    PyMem_Free(rects);
    if(!ok) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowDisplayMode(PySDL_Window *self, PyObject *arg) {
    SDL_DisplayMode mode;
    SDL_DisplayMode *mp = NULL;
    if(arg != Py_None) {
        SDL_memset(&mode, 0, sizeof(mode));
        int fmt = 0;
        float rate = 0.0f;
        if(!PyArg_ParseTuple(arg, "iii|f", &fmt, &mode.w, &mode.h, &rate)) {
            return NULL;
        }
        mode.format = (SDL_PixelFormat)fmt;
        mode.refresh_rate = rate;
        mp = &mode;
    }
    if(!SDL_SetWindowFullscreenMode(self->window, mp)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_GetWindowDisplayMode(PySDL_Window *self, PyObject *ign) {
    const SDL_DisplayMode *mode = SDL_GetWindowFullscreenMode(self->window);
    if(NULL == mode) {
        Py_RETURN_NONE;
    }
    return Py_BuildValue("(iiif)", mode->format, mode->w, mode->h, mode->refresh_rate);
}

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
    if(!SDL_FlashWindow(self->window, (SDL_FlashOperation)op)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Window_SetWindowMouseRect(PySDL_Window *self, PyObject *arg) {
    SDL_Rect rect;
    SDL_Rect *rp = NULL;
    if(arg != Py_None) {
        if(!PyToRect(arg, &rect)) {
            return NULL;
        }
        rp = &rect;
    }
    if(!SDL_SetWindowMouseRect(self->window, rp)) {
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

//=========================================================
// vulkan / metal
//=========================================================

static PyObject * PySDL_Window_Vulkan_CreateSurface(PySDL_Window *self, PyObject *arg) {
    // `arg` is a VkInstance handle as an int (e.g. from the `vulkan` package).
    unsigned long long instance_handle = PyLong_AsUnsignedLongLong(arg);
    if((unsigned long long)-1 == instance_handle && PyErr_Occurred()) {
        return NULL;
    }
    VkSurfaceKHR surface = 0;
    if(!SDL_Vulkan_CreateSurface(self->window,
        (VkInstance)(uintptr_t)instance_handle, NULL, &surface)) {
        return _wraise();
    }
    return PyLong_FromUnsignedLongLong((unsigned long long)(uintptr_t)surface);
}

static PyObject * PySDL_Window_Metal_CreateView(PySDL_Window *self, PyObject *ign) {
    // The window must have been created with SDL_WINDOW_METAL. Returns the
    // SDL_MetalView handle as an int; pass it to SDL3.Metal_GetLayer /
    // SDL3.Metal_DestroyView.
    SDL_MetalView view = SDL_Metal_CreateView(self->window);
    if(NULL == view) {
        return _wraise();
    }
    return PyLong_FromVoidPtr(view);
}

//=========================================================
// renderer, ICC profile, window surface, shape
//=========================================================

// The Renderer object that owns this window's renderer (the same object
// CreateRenderer / SDL3.Renderer / CreateWindowAndRenderer returned), or None.
// SDL's own renderer behind an accelerated window surface is not exposed.
static PyObject * PySDL_Window_GetRenderer(PySDL_Window *self, PyObject *ign) {
    if(NULL == self->window) {
        PyErr_SetString(pysdl_Error, "Invalid window");
        return NULL;
    }
    return PySDL_RendererFor(SDL_GetRenderer(self->window));
}

// The ICC profile of the window's display as bytes, or None if there is none.
static PyObject * PySDL_Window_GetWindowICCProfile(PySDL_Window *self, PyObject *ign) {
    size_t size = 0;
    void *data = SDL_GetWindowICCProfile(self->window, &size);
    if(NULL == data) {
        SDL_ClearError();
        Py_RETURN_NONE;
    }
    PyObject *result = PyBytes_FromStringAndSize(data, (Py_ssize_t)size);
    SDL_free(data);
    return result;
}

static PyObject * PySDL_Window_HasWindowSurface(PySDL_Window *self, PyObject *ign) {
    return PyBool_FromLong(SDL_WindowHasSurface(self->window));
}

// Free the window surface; a Surface from GetWindowSurface() is emptied (0x0)
// first. Refused while a Renderer draws to the window (a software one renders
// into this very surface).
static PyObject * PySDL_Window_DestroyWindowSurface(PySDL_Window *self, PyObject *ign) {
    if(NULL != self->window && PySDL_RendererOwned(SDL_GetRenderer(self->window))) {
        PyErr_SetString(pysdl_Error, "the window has a Renderer: destroy it first");
        return NULL;
    }
    if(NULL != self->window) {
        _drop_surface(SDL_GetWindowID(self->window));
    }
    if(!SDL_DestroyWindowSurface(self->window)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}

// SetWindowShape(surface | None): the alpha channel of `surface` shapes a
// window created with WINDOW_TRANSPARENT (SDL copies it); None removes it,
// which the linked SDL only supports from 3.4.4 (earlier ones reject NULL).
static PyObject * PySDL_Window_SetWindowShape(PySDL_Window *self, PyObject *arg) {
    SDL_Surface *shape = NULL;
    if(arg != Py_None && !PySDL_SurfaceArg(arg, &shape)) {
        return NULL;
    }
    if(NULL == shape && SDL_GetVersion() < SDL_VERSIONNUM(3, 4, 4)) {
        PyErr_SetString(pysdl_Error, "removing a window shape (None) needs SDL 3.4.4");
        return NULL;
    }
    if(!SDL_SetWindowShape(self->window, shape)) {
        return _wraise();
    }
    Py_RETURN_NONE;
}
