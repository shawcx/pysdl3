#include "pysdl.h"

PyObject *pysdl_Error;

static PyObject * PySDL_Init                  (PyObject*, PyObject*);
static PyObject * PySDL_InitSubSystem         (PyObject*, PyObject*);
static PyObject * PySDL_QuitSubSystem         (PyObject*, PyObject*);
static PyObject * PySDL_WasInit               (PyObject*, PyObject*);
static PyObject * PySDL_Quit                  (PyObject*, PyObject*);
static PyObject * PySDL_GetError              (PyObject*, PyObject*);
static PyObject * PySDL_Version               (PyObject*, PyObject*);
static PyObject * PySDL_GetPlatform           (PyObject*, PyObject*);
static PyObject * PySDL_GetCurrentVideoDriver (PyObject*, PyObject*);
static PyObject * PySDL_GetVideoDrivers       (PyObject*, PyObject*);
static PyObject * PySDL_GetTicks              (PyObject*, PyObject*);
static PyObject * PySDL_LoadBMP               (PyObject*, PyObject*);
static PyObject * PySDL_ShowCursor            (PyObject*, PyObject*);

static PyObject * _wrap_surface (SDL_Surface*, Py_buffer*);

static PyObject * PySDL_CreateRGBSurface             (PyObject*, PyObject*, PyObject *);
static PyObject * PySDL_CreateRGBSurfaceFrom         (PyObject*, PyObject*, PyObject *);
static PyObject * PySDL_CreateRGBSurfaceWithFormat     (PyObject*, PyObject*, PyObject *);
static PyObject * PySDL_CreateRGBSurfaceWithFormatFrom (PyObject*, PyObject*, PyObject *);

static PyObject * PySDL_GetPixelFormatName     (PyObject*, PyObject*);
static PyObject * PySDL_PixelFormatEnumToMasks (PyObject*, PyObject*);
static PyObject * PySDL_MasksToPixelFormatEnum (PyObject*, PyObject*);
static PyObject * PySDL_ConvertPixels          (PyObject*, PyObject*, PyObject*);
static PyObject * PySDL_PremultiplyAlpha       (PyObject*, PyObject*, PyObject*);

static PyObject * PySDL_CreateSoftwareRenderer (PyObject*, PyObject*);
static PyObject * PySDL_ComposeCustomBlendMode (PyObject*, PyObject*);

static PyObject * PySDL_GetCPUCount           (PyObject*, PyObject*);
static PyObject * PySDL_GetCPUCacheLineSize   (PyObject*, PyObject*);
static PyObject * PySDL_HasAVX                (PyObject*, PyObject*);
static PyObject * PySDL_HasAVX2               (PyObject*, PyObject*);
static PyObject * PySDL_HasAVX512F            (PyObject*, PyObject*);
static PyObject * PySDL_HasAltiVec            (PyObject*, PyObject*);
static PyObject * PySDL_HasSSE                (PyObject*, PyObject*);
static PyObject * PySDL_HasSSE2               (PyObject*, PyObject*);
static PyObject * PySDL_HasSSE3               (PyObject*, PyObject*);
static PyObject * PySDL_HasSSE41              (PyObject*, PyObject*);
static PyObject * PySDL_HasSSE42              (PyObject*, PyObject*);
static PyObject * PySDL_HasNEON               (PyObject*, PyObject*);
static PyObject * PySDL_HasLSX                (PyObject*, PyObject*);
static PyObject * PySDL_HasLASX               (PyObject*, PyObject*);
static PyObject * PySDL_GetSystemRAM          (PyObject*, PyObject*);
static PyObject * PySDL_SIMDGetAlignment      (PyObject*, PyObject*);
static PyObject * PySDL_GetPowerInfo          (PyObject*, PyObject*);

static PyObject * PySDL_GetNumVideoDisplays   (PyObject*, PyObject*);
static PyObject * PySDL_GetDisplays           (PyObject*, PyObject*);
static PyObject * PySDL_GetDisplayMode        (PyObject*, PyObject*);
static PyObject * PySDL_GetDesktopDisplayMode (PyObject*, PyObject*);
static PyObject * PySDL_GetCurrentDisplayMode (PyObject*, PyObject*);
static PyObject * PySDL_GetDisplayBounds      (PyObject*, PyObject*);

static PyObject * PySDL_GL_SetAttribute       (PyObject*, PyObject*);
static PyObject * PySDL_GL_GetAttribute       (PyObject*, PyObject*);
static PyObject * PySDL_GL_ResetAttributes    (PyObject*, PyObject*);
static PyObject * PySDL_GL_ExtensionSupported (PyObject*, PyObject*);
static PyObject * PySDL_GL_SetSwapInterval    (PyObject*, PyObject*);
static PyObject * PySDL_GL_GetSwapInterval    (PyObject*, PyObject*);

static PyObject * PySDL_GetNumRenderDrivers   (PyObject*, PyObject*);
static PyObject * PySDL_GetRenderDriverInfo   (PyObject*, PyObject*);

static PyObject * PySDL_GetNumAudioDevices    (PyObject*, PyObject*);
static PyObject * PySDL_GetAudioDeviceName    (PyObject*, PyObject*);

static PyObject * PySDL_Delay                    (PyObject*, PyObject*);
static PyObject * PySDL_GetPerformanceCounter    (PyObject*, PyObject*);
static PyObject * PySDL_GetPerformanceFrequency  (PyObject*, PyObject*);
static PyObject * PySDL_GetVersion               (PyObject*, PyObject*);
static PyObject * PySDL_GetRevision              (PyObject*, PyObject*);
static PyObject * PySDL_SetError                 (PyObject*, PyObject*);
static PyObject * PySDL_ClearError               (PyObject*, PyObject*);
static PyObject * PySDL_GetBasePath              (PyObject*, PyObject*);
static PyObject * PySDL_GetPrefPath              (PyObject*, PyObject*);
static PyObject * PySDL_EnableScreenSaver        (PyObject*, PyObject*);
static PyObject * PySDL_DisableScreenSaver       (PyObject*, PyObject*);
static PyObject * PySDL_IsScreenSaverEnabled     (PyObject*, PyObject*);
static PyObject * PySDL_GetClipboardText         (PyObject*, PyObject*);
static PyObject * PySDL_SetClipboardText         (PyObject*, PyObject*);
static PyObject * PySDL_HasClipboardText         (PyObject*, PyObject*);
static PyObject * PySDL_GetPrimarySelectionText  (PyObject*, PyObject*);
static PyObject * PySDL_SetPrimarySelectionText  (PyObject*, PyObject*);
static PyObject * PySDL_HasPrimarySelectionText  (PyObject*, PyObject*);

static PyMethodDef pysdl_PyMethodDefs[] = {
    { "Init",                  PySDL_Init,                  METH_VARARGS },
    { "InitSubSystem",         PySDL_InitSubSystem,         METH_O       },
    { "QuitSubSystem",         PySDL_QuitSubSystem,         METH_O       },
    { "WasInit",               PySDL_WasInit,               METH_VARARGS },
    { "Quit",                  PySDL_Quit,                  METH_NOARGS  },
    { "GetError",              PySDL_GetError,              METH_NOARGS  },
    { "GetErrorMsg",           PySDL_GetError,              METH_NOARGS  },  // pysdl2 name; SDL3's GetError is per-thread already
    { "Version",               PySDL_Version,               METH_NOARGS  },
    { "GetPlatform",           PySDL_GetPlatform,           METH_NOARGS  },
    { "GetCurrentVideoDriver", PySDL_GetCurrentVideoDriver, METH_NOARGS  },
    { "GetVideoDrivers",       PySDL_GetVideoDrivers,       METH_NOARGS  },
    { "GetTicks",              PySDL_GetTicks,              METH_NOARGS  },
    { "LoadBMP",               PySDL_LoadBMP,               METH_O       },
    { "ShowCursor",            PySDL_ShowCursor,            METH_O       },

    { "CreateRGBSurface",     (PyCFunction)PySDL_CreateRGBSurface,     METH_VARARGS | METH_KEYWORDS },
    { "CreateRGBSurfaceFrom", (PyCFunction)PySDL_CreateRGBSurfaceFrom, METH_VARARGS | METH_KEYWORDS },
    { "CreateRGBSurfaceWithFormat",     (PyCFunction)PySDL_CreateRGBSurfaceWithFormat,     METH_VARARGS | METH_KEYWORDS },
    { "CreateRGBSurfaceWithFormatFrom", (PyCFunction)PySDL_CreateRGBSurfaceWithFormatFrom, METH_VARARGS | METH_KEYWORDS },
    { "CreateSurface",        (PyCFunction)PySDL_CreateRGBSurfaceWithFormat,     METH_VARARGS | METH_KEYWORDS },
    { "CreateSurfaceFrom",    (PyCFunction)PySDL_CreateRGBSurfaceWithFormatFrom, METH_VARARGS | METH_KEYWORDS },

    { "GetPixelFormatName",     PySDL_GetPixelFormatName,     METH_O       },
    { "PixelFormatEnumToMasks", PySDL_PixelFormatEnumToMasks, METH_O       },
    { "MasksToPixelFormatEnum", PySDL_MasksToPixelFormatEnum, METH_VARARGS },
    { "ConvertPixels",          (PyCFunction)PySDL_ConvertPixels,    METH_VARARGS | METH_KEYWORDS },
    { "PremultiplyAlpha",       (PyCFunction)PySDL_PremultiplyAlpha, METH_VARARGS | METH_KEYWORDS },

    { "CreateSoftwareRenderer", PySDL_CreateSoftwareRenderer, METH_O       },
    { "ComposeCustomBlendMode", PySDL_ComposeCustomBlendMode, METH_VARARGS },

    { "GetCPUCount",           PySDL_GetCPUCount,           METH_NOARGS  },
    { "GetCPUCacheLineSize",   PySDL_GetCPUCacheLineSize,   METH_NOARGS  },
    { "HasAVX",                PySDL_HasAVX,                METH_NOARGS  },
    { "HasAVX2",               PySDL_HasAVX2,               METH_NOARGS  },
    { "HasAVX512F",            PySDL_HasAVX512F,            METH_NOARGS  },
    { "HasAltiVec",            PySDL_HasAltiVec,            METH_NOARGS  },
    { "HasSSE",                PySDL_HasSSE,                METH_NOARGS  },
    { "HasSSE2",               PySDL_HasSSE2,               METH_NOARGS  },
    { "HasSSE3",               PySDL_HasSSE3,               METH_NOARGS  },
    { "HasSSE41",              PySDL_HasSSE41,              METH_NOARGS  },
    { "HasSSE42",              PySDL_HasSSE42,              METH_NOARGS  },
    { "HasNEON",               PySDL_HasNEON,               METH_NOARGS  },
    { "HasLSX",                PySDL_HasLSX,                METH_NOARGS  },
    { "HasLASX",               PySDL_HasLASX,               METH_NOARGS  },
    { "GetSystemRAM",          PySDL_GetSystemRAM,          METH_NOARGS  },
    { "SIMDGetAlignment",      PySDL_SIMDGetAlignment,      METH_NOARGS  },
    { "GetPowerInfo",          PySDL_GetPowerInfo,          METH_NOARGS  },

    { "GetNumVideoDisplays",   PySDL_GetNumVideoDisplays,   METH_NOARGS  },
    { "GetDisplays",           PySDL_GetDisplays,           METH_NOARGS  },
    { "GetDisplayMode",        PySDL_GetDisplayMode,        METH_VARARGS },
    { "GetDesktopDisplayMode", PySDL_GetDesktopDisplayMode, METH_O       },
    { "GetCurrentDisplayMode", PySDL_GetCurrentDisplayMode, METH_O       },
    { "GetDisplayBounds",      PySDL_GetDisplayBounds,      METH_O       },

    { "GL_SetAttribute",       PySDL_GL_SetAttribute,       METH_VARARGS },
    { "GL_GetAttribute",       PySDL_GL_GetAttribute,       METH_O       },
    { "GL_ResetAttributes",    PySDL_GL_ResetAttributes,    METH_NOARGS  },
    { "GL_ExtensionSupported", PySDL_GL_ExtensionSupported, METH_O       },
    { "GL_SetSwapInterval",    PySDL_GL_SetSwapInterval,    METH_O       },
    { "GL_GetSwapInterval",    PySDL_GL_GetSwapInterval,    METH_NOARGS  },

    { "GetNumRenderDrivers",   PySDL_GetNumRenderDrivers,   METH_NOARGS  },
    { "GetRenderDriverInfo",   PySDL_GetRenderDriverInfo,   METH_O       },

    { "GetNumAudioDevices",    PySDL_GetNumAudioDevices,    METH_VARARGS },
    { "GetAudioDeviceName",    PySDL_GetAudioDeviceName,    METH_VARARGS },

    { "Delay",                    PySDL_Delay,                    METH_O       },
    { "GetPerformanceCounter",    PySDL_GetPerformanceCounter,    METH_NOARGS  },
    { "GetPerformanceFrequency",  PySDL_GetPerformanceFrequency,  METH_NOARGS  },
    { "GetVersion",               PySDL_GetVersion,               METH_NOARGS  },
    { "GetRevision",              PySDL_GetRevision,              METH_NOARGS  },
    { "SetError",                 PySDL_SetError,                 METH_O       },
    { "ClearError",               PySDL_ClearError,               METH_NOARGS  },
    { "GetBasePath",              PySDL_GetBasePath,              METH_NOARGS  },
    { "GetPrefPath",              PySDL_GetPrefPath,              METH_VARARGS },
    { "EnableScreenSaver",        PySDL_EnableScreenSaver,        METH_NOARGS  },
    { "DisableScreenSaver",       PySDL_DisableScreenSaver,       METH_NOARGS  },
    { "IsScreenSaverEnabled",     PySDL_IsScreenSaverEnabled,     METH_NOARGS  },
    { "GetClipboardText",         PySDL_GetClipboardText,         METH_NOARGS  },
    { "SetClipboardText",         PySDL_SetClipboardText,         METH_O       },
    { "HasClipboardText",         PySDL_HasClipboardText,         METH_NOARGS  },
    { "GetPrimarySelectionText",  PySDL_GetPrimarySelectionText,  METH_NOARGS  },
    { "SetPrimarySelectionText",  PySDL_SetPrimarySelectionText,  METH_O       },
    { "HasPrimarySelectionText",  PySDL_HasPrimarySelectionText,  METH_NOARGS  },

    { NULL }
};

static PyModuleDef pysdl_module = {
    PyModuleDef_HEAD_INIT,
    "SDL3",
    NULL,
    -1,
    pysdl_PyMethodDefs
};

// Register a constructible wrapper type on the module (with an optional alias).
static int _add_type(PyObject *module, PyTypeObject *type, const char *name, const char *alias) {
    if(0 > PyType_Ready(type)) {
        return -1;
    }
    Py_INCREF(type);
    if(0 > PyModule_AddObject(module, name, (PyObject *)type)) {
        return -1;
    }
    if(alias) {
        Py_INCREF(type);
        if(0 > PyModule_AddObject(module, alias, (PyObject *)type)) {
            return -1;
        }
    }
    return 0;
}

PyMODINIT_FUNC PyInit_SDL3(void) {
    PyObject *module = PyModule_Create(&pysdl_module);
    if(NULL == module) {
        return NULL;
    }

    pysdl_Error = PyErr_NewException("SDL3.error", NULL, NULL);
    Py_INCREF(pysdl_Error);
    PyModule_AddObject(module, "error", pysdl_Error);

    if(0 > _add_type(module, &PySDL_Window_Type,      "Window",      NULL)
        || 0 > _add_type(module, &PySDL_Renderer_Type,     "Renderer",     NULL)
        || 0 > _add_type(module, &PySDL_Texture_Type,      "Texture",      NULL)
        || 0 > _add_type(module, &PySDL_PixelFormat_Type,  "PixelFormat",  NULL)
        || 0 > _add_type(module, &PySDL_Palette_Type,      "Palette",      NULL)
        || 0 > _add_type(module, &PySDL_Audio_Type,        "Audio",        NULL)
        || 0 > _add_type(module, &PySDL_AudioStream_Type,  "AudioStream",  NULL)
        || 0 > _add_type(module, &PySDL_Cursor_Type,       "Cursor",       NULL)
        || 0 > _add_type(module, &PySDL_Joystick_Type,     "Joystick",     NULL)
        || 0 > _add_type(module, &PySDL_Gamepad_Type,      "Gamepad",      "GameController")
        || 0 > _add_type(module, &PySDL_Timer_Type,        "Timer",        NULL)
        || 0 > _add_type(module, &PySDL_Haptic_Type,       "Haptic",       NULL)
        || 0 > _add_type(module, &PySDL_Sensor_Type,       "Sensor",       NULL)) {
        return NULL;
    }

    // PixelFormat / Palette are the only extra types not exposed above; already done.
    if(0 > PyType_Ready(&PySDL_Surface_Type)) {
        return NULL;
    }
    Py_INCREF(&PySDL_Surface_Type);

    if(0 > PyModule_AddFunctions(module, pysdl_events_methods)
        || 0 > PyModule_AddFunctions(module, pysdl_input_methods)
        || 0 > PyModule_AddFunctions(module, pysdl_cursor_methods)
        || 0 > PyModule_AddFunctions(module, pysdl_joystick_methods)
        || 0 > PyModule_AddFunctions(module, pysdl_gamepad_methods)
        || 0 > PyModule_AddFunctions(module, pysdl_haptic_methods)
        || 0 > PyModule_AddFunctions(module, pysdl_sensor_methods)
        || 0 > PyModule_AddFunctions(module, pysdl_video_methods)
        || 0 > PyModule_AddFunctions(module, pysdl_audio_methods)
        || 0 > PyModule_AddFunctions(module, pysdl_rect_methods)
        || 0 > PyModule_AddFunctions(module, pysdl_image_methods)) {
        return NULL;
    }

#ifdef PYSDL_HAVE_TTF
    if(0 > _add_type(module, &PySDL_Font_Type, "Font", NULL)
        || 0 > PyModule_AddFunctions(module, pysdl_ttf_methods)
        || 0 > PySDL_TTF_Constants(module)) {
        return NULL;
    }
#endif

    _constants(module);

    return module;
}

PyObject * PySDL_New(PyTypeObject *type) {
    PyObject *obj = PyObject_CallObject((PyObject *)type, NULL);
    if(NULL == obj) {
        PyErr_Format(PyExc_TypeError, "Could not create %s object", type->tp_name);
    }
    return obj;
}

//=========================================================
// init / errors / version
//=========================================================

// SDL3 dropped SDL_INIT_EVERYTHING; the no-arg default matches the synthesized
// INIT_EVERYTHING constant.
#define PYSDL_INIT_DEFAULT \
    (SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD)

// Init / Quit drop the GIL around SDL: shutting a subsystem down waits for
// SDL threads that may be inside a Python callback waiting for the GIL (the
// timer thread joined by SDL_QuitTimers, an audio device thread holding its
// device while SDL_QuitAudio closes it, a virtual joystick's callback holding
// the joystick lock that init / quit take). The binding's own bookkeeping
// (what was running, invalidating wrappers) happens with the GIL held, before
// and after. Other threads must not use SDL while these run.
static PyObject * PySDL_Init(PyObject *self, PyObject *args) {
    unsigned int flags = PYSDL_INIT_DEFAULT;
    if(!PyArg_ParseTuple(args, "|I", &flags)) {
        return NULL;
    }
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_Init((SDL_InitFlags)flags);
    Py_END_ALLOW_THREADS
    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_WasInit(PyObject *self, PyObject *args) {
    unsigned int flags = 0;
    if(!PyArg_ParseTuple(args, "|I", &flags)) {
        return NULL;
    }
    return PyLong_FromUnsignedLong(SDL_WasInit((SDL_InitFlags)flags));
}

static PyObject * PySDL_InitSubSystem(PyObject *self, PyObject *arg) {
    unsigned long flags = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == flags && PyErr_Occurred()) {
        return NULL;
    }
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_InitSubSystem((SDL_InitFlags)flags);
    Py_END_ALLOW_THREADS
    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_QuitSubSystem(PyObject *self, PyObject *arg) {
    unsigned long flags = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == flags && PyErr_Occurred()) {
        return NULL;
    }
    // Subsystems are refcounted: only if video was running and really stopped
    // (a software renderer on a Surface lives without video). Likewise the
    // event subsystem, which takes the event filter and watchers with it.
    bool video = NULL != SDL_GetCurrentVideoDriver();
    bool events = 0 != SDL_WasInit(SDL_INIT_EVENTS);
    Py_BEGIN_ALLOW_THREADS
        SDL_QuitSubSystem((SDL_InitFlags)flags);
    Py_END_ALLOW_THREADS
    if(video) {
        PySDL_CheckVideoGone();
    }
    if(events && 0 == SDL_WasInit(SDL_INIT_EVENTS)) {
        PySDL_EventWatchesGone();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Quit(PyObject *self, PyObject *ign) {
    bool events = 0 != SDL_WasInit(SDL_INIT_EVENTS);
    Py_BEGIN_ALLOW_THREADS
        SDL_Quit();
    Py_END_ALLOW_THREADS
    PySDL_InvalidateWindows();  // SDL_Quit destroyed every window, renderer and cursor
    if(events) {
        PySDL_EventWatchesGone();
    }
    PySDL_HintCallbacksGone();  // SDL_QuitHints freed them
    Py_RETURN_NONE;
}

static PyObject * PySDL_GetError(PyObject *self, PyObject *ign) {
    return PyUnicode_FromString(SDL_GetError());
}

static PyObject * PySDL_Version(PyObject *self, PyObject *ign) {
    return Py_BuildValue("(iii)", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION);
}

static PyObject * PySDL_GetPlatform(PyObject *self, PyObject *ign) {
    return PyUnicode_FromString(SDL_GetPlatform());
}

static PyObject * PySDL_GetCurrentVideoDriver(PyObject *self, PyObject *ign) {
    const char *name = SDL_GetCurrentVideoDriver();
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_GetVideoDrivers(PyObject *self, PyObject *ign) {
    int count = SDL_GetNumVideoDrivers();
    PyObject *list = PyList_New(count < 0 ? 0 : count);
    if(NULL == list) {
        return NULL;
    }
    for(int idx = 0; idx < count; ++idx) {
        PyList_SET_ITEM(list, idx, PyUnicode_FromString(SDL_GetVideoDriver(idx)));
    }
    return list;
}

static PyObject * PySDL_GetTicks(PyObject *self, PyObject *ign) {
    return PyLong_FromUnsignedLongLong(SDL_GetTicks());
}

//=========================================================
// BMP loader  (LoadImage and the rest of SDL_image: pysdl_Image.c)
//=========================================================

// Takes a path (str / os.PathLike) or the file's bytes.
static PyObject * PySDL_LoadBMP(PyObject *self, PyObject *arg) {
    Py_buffer view;
    SDL_IOStream *io = PySDL_IOFromObject(arg, &view);
    if(NULL == io) {
        return NULL;
    }
    SDL_Surface *surface;
    Py_BEGIN_ALLOW_THREADS
        surface = SDL_LoadBMP_IO(io, true);
    Py_END_ALLOW_THREADS
    PyBuffer_Release(&view);
    return _wrap_surface(surface, NULL);
}

static PyObject * PySDL_ShowCursor(PyObject *self, PyObject *args) {
    long mode = PyLong_AsLong(args);
    if(-1 == mode && PyErr_Occurred()) {
        return NULL;
    }
    if(mode < 0) {
        return PyBool_FromLong(SDL_CursorVisible());
    }
    if(mode ? !SDL_ShowCursor() : !SDL_HideCursor()) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

//=========================================================
// surface factories  (SDL3: SDL_CreateSurface / SDL_CreateSurfaceFrom)
//=========================================================

static PyObject * _wrap_surface(SDL_Surface *surface, Py_buffer *pixels) {
    if(NULL == surface) {
        if(pixels) {
            PyBuffer_Release(pixels);
        }
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    PySDL_Surface *wrapper = (PySDL_Surface *)PySDL_New(&PySDL_Surface_Type);
    if(NULL == wrapper) {
        SDL_DestroySurface(surface);
        if(pixels) {
            PyBuffer_Release(pixels);
        }
        return NULL;
    }
    wrapper->surface = surface;
    if(pixels) {
        wrapper->pixels = *pixels;  // keep the caller's buffer alive
    }
    return (PyObject *)wrapper;
}

static PyObject * PySDL_CreateRGBSurface(PyObject *self, PyObject *args, PyObject *kwds) {
    int w = 0, h = 0, d = 32;
    uint32_t rmask = 0xff000000, gmask = 0x00ff0000, bmask = 0x0000ff00, amask = 0x000000ff;

    static char *kwlist[] = {"size", "depth", "rmask", "gmask", "bmask", "amask", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "(ii)|iIIII", kwlist,
        &w, &h, &d, &rmask, &gmask, &bmask, &amask)) {
        return NULL;
    }

    SDL_PixelFormat format = SDL_GetPixelFormatForMasks(d, rmask, gmask, bmask, amask);
    return _wrap_surface(SDL_CreateSurface(w, h, format), NULL);
}

static PyObject * PySDL_CreateRGBSurfaceFrom(PyObject *self, PyObject *args, PyObject *kwds) {
    Py_buffer pixels;
    int w = 0, h = 0, d = 32, pitch = 0;
    uint32_t rmask = 0xff000000, gmask = 0x00ff0000, bmask = 0x0000ff00, amask = 0x000000ff;

    static char *kwlist[] = {"pixels", "size", "depth", "rmask", "gmask", "bmask", "amask", "pitch", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "y*(ii)|iIIIIi", kwlist,
        &pixels, &w, &h, &d, &rmask, &gmask, &bmask, &amask, &pitch)) {
        return NULL;
    }

    SDL_PixelFormat format = SDL_GetPixelFormatForMasks(d, rmask, gmask, bmask, amask);
    if(pitch <= 0) {
        pitch = w * SDL_BYTESPERPIXEL(format);
    }
    return _wrap_surface(SDL_CreateSurfaceFrom(w, h, format, pixels.buf, pitch), &pixels);
}

static PyObject * PySDL_CreateRGBSurfaceWithFormat(PyObject *self, PyObject *args, PyObject *kwds) {
    int w = 0, h = 0, depth = 0;
    unsigned int format;

    static char *kwlist[] = {"size", "format", "depth", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "(ii)I|i", kwlist, &w, &h, &format, &depth)) {
        return NULL;
    }
    return _wrap_surface(SDL_CreateSurface(w, h, (SDL_PixelFormat)format), NULL);
}

static PyObject * PySDL_CreateRGBSurfaceWithFormatFrom(PyObject *self, PyObject *args, PyObject *kwds) {
    Py_buffer pixels;
    int w = 0, h = 0, depth = 0, pitch = 0;
    unsigned int format;

    static char *kwlist[] = {"pixels", "size", "format", "depth", "pitch", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "y*(ii)I|ii", kwlist,
        &pixels, &w, &h, &format, &depth, &pitch)) {
        return NULL;
    }
    if(pitch <= 0) {
        pitch = w * SDL_BYTESPERPIXEL(format);
    }
    return _wrap_surface(SDL_CreateSurfaceFrom(w, h, (SDL_PixelFormat)format, pixels.buf, pitch), &pixels);
}

static PyObject * PySDL_GetPixelFormatName(PyObject *self, PyObject *arg) {
    unsigned long format = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == format && PyErr_Occurred()) {
        return NULL;
    }
    return PyUnicode_FromString(SDL_GetPixelFormatName((SDL_PixelFormat)format));
}

static PyObject * PySDL_PixelFormatEnumToMasks(PyObject *self, PyObject *arg) {
    unsigned long format = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == format && PyErr_Occurred()) {
        return NULL;
    }

    int bpp = 0;
    Uint32 rmask, gmask, bmask, amask;
    if(!SDL_GetMasksForPixelFormat((SDL_PixelFormat)format, &bpp, &rmask, &gmask, &bmask, &amask)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return Py_BuildValue("(iIIII)", bpp, rmask, gmask, bmask, amask);
}

static PyObject * PySDL_MasksToPixelFormatEnum(PyObject *self, PyObject *args) {
    int bpp;
    uint32_t rmask, gmask, bmask, amask;
    if(!PyArg_ParseTuple(args, "iIIII", &bpp, &rmask, &gmask, &bmask, &amask)) {
        return NULL;
    }
    return PyLong_FromUnsignedLong(SDL_GetPixelFormatForMasks(bpp, rmask, gmask, bmask, amask));
}

// ConvertPixels(size, src_format, src, dst_format, src_pitch=0, dst_pitch=0)
// and PremultiplyAlpha(..., linear=False) -> the converted pixels as bytes.
// A pitch of 0 is tight rows; YUV layouts follow SDL's (chroma planes after
// the luma rows). `src` must hold the whole image before SDL reads it.
static PyObject * _convert(PyObject *args, PyObject *kwds, int premultiply) {
    int w, h, src_pitch = 0, dst_pitch = 0, linear = 0;
    unsigned int src_format, dst_format;
    Py_buffer src;
    static char *kwlist[] = {"size", "src_format", "src", "dst_format", "src_pitch", "dst_pitch", NULL};
    static char *kwlist_linear[] = {"size", "src_format", "src", "dst_format", "src_pitch", "dst_pitch", "linear", NULL};
    if(premultiply ? !PyArg_ParseTupleAndKeywords(args, kwds, "(ii)Iy*I|iip", kwlist_linear,
                         &w, &h, &src_format, &src, &dst_format, &src_pitch, &dst_pitch, &linear)
                   : !PyArg_ParseTupleAndKeywords(args, kwds, "(ii)Iy*I|ii", kwlist,
                         &w, &h, &src_format, &src, &dst_format, &src_pitch, &dst_pitch)) {
        return NULL;
    }

    PyObject *dst = NULL;
    if(w <= 0 || h <= 0 || src_pitch < 0 || dst_pitch < 0) {
        PyErr_SetString(PyExc_ValueError, "size must be positive and pitches non-negative");
        goto done;
    }
    Py_ssize_t need = PySDL_PixelsSize((SDL_PixelFormat)src_format, w, h, &src_pitch);
    if(0 > need) {
        goto done;
    }
    if(src.len < need) {
        PyErr_Format(PyExc_ValueError, "src holds %zd bytes, needs %zd", src.len, need);
        goto done;
    }
    Py_ssize_t size = PySDL_PixelsSize((SDL_PixelFormat)dst_format, w, h, &dst_pitch);
    if(0 > size) {
        goto done;
    }
    dst = PyBytes_FromStringAndSize(NULL, size);
    if(NULL == dst) {
        goto done;
    }

    bool ok = premultiply
        ? SDL_PremultiplyAlpha(w, h, (SDL_PixelFormat)src_format, src.buf, src_pitch,
              (SDL_PixelFormat)dst_format, PyBytes_AS_STRING(dst), dst_pitch, linear ? true : false)
        : SDL_ConvertPixels(w, h, (SDL_PixelFormat)src_format, src.buf, src_pitch,
              (SDL_PixelFormat)dst_format, PyBytes_AS_STRING(dst), dst_pitch);
    if(!ok) {
        Py_CLEAR(dst);
        PyErr_SetString(pysdl_Error, SDL_GetError());
    }

done:
    PyBuffer_Release(&src);
    return dst;
}

static PyObject * PySDL_ConvertPixels(PyObject *self, PyObject *args, PyObject *kwds) {
    return _convert(args, kwds, 0);
}

static PyObject * PySDL_PremultiplyAlpha(PyObject *self, PyObject *args, PyObject *kwds) {
    return _convert(args, kwds, 1);
}

//=========================================================
// renderers / blend modes
//=========================================================

// CreateSoftwareRenderer(surface): a renderer drawing into `surface`, which
// it keeps alive (SDL holds the pointer). Only a Surface the binding owns: SDL
// frees a window surface or a LockToSurface surface under the renderer (draw
// to a window with Window.CreateRenderer("software") instead).
static PyObject * PySDL_CreateSoftwareRenderer(PyObject *self, PyObject *arg) {
    SDL_Surface *surface;
    if(!PySDL_SurfaceArg(arg, &surface)) {
        return NULL;
    }
    if(!((PySDL_Surface *)arg)->shouldFree) {
        PyErr_SetString(pysdl_Error, "SDL owns this Surface (a window or locked-texture surface)");
        return NULL;
    }

    PySDL_Renderer *pysdl_Renderer = (PySDL_Renderer *)PySDL_New(&PySDL_Renderer_Type);
    if(NULL == pysdl_Renderer) {
        return NULL;
    }

    pysdl_Renderer->renderer = SDL_CreateSoftwareRenderer(surface);
    if(NULL == pysdl_Renderer->renderer) {
        Py_DECREF(pysdl_Renderer);
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    pysdl_Renderer->surface = Py_NewRef(arg);

    return (PyObject *)pysdl_Renderer;
}

static PyObject * PySDL_ComposeCustomBlendMode(PyObject *self, PyObject *args) {
    int srcColor, dstColor, colorOp, srcAlpha, dstAlpha, alphaOp;
    if(!PyArg_ParseTuple(args, "iiiiii",
        &srcColor, &dstColor, &colorOp, &srcAlpha, &dstAlpha, &alphaOp)) {
        return NULL;
    }
    SDL_BlendMode mode = SDL_ComposeCustomBlendMode(
        (SDL_BlendFactor)srcColor, (SDL_BlendFactor)dstColor, (SDL_BlendOperation)colorOp,
        (SDL_BlendFactor)srcAlpha, (SDL_BlendFactor)dstAlpha, (SDL_BlendOperation)alphaOp);
    return PyLong_FromUnsignedLong(mode);
}

//=========================================================
// CPU info / power
//=========================================================

static PyObject * PySDL_GetCPUCount(PyObject *self, PyObject *ign) {
    return PyLong_FromLong(SDL_GetNumLogicalCPUCores());
}

static PyObject * PySDL_GetCPUCacheLineSize(PyObject *self, PyObject *ign) {
    return PyLong_FromLong(SDL_GetCPUCacheLineSize());
}

#define PYSDL_HAS(name, call) \
    static PyObject * name(PyObject *self, PyObject *ign) { return PyBool_FromLong(call()); }

PYSDL_HAS(PySDL_HasAVX,      SDL_HasAVX)
PYSDL_HAS(PySDL_HasAVX2,     SDL_HasAVX2)
PYSDL_HAS(PySDL_HasAVX512F,  SDL_HasAVX512F)
PYSDL_HAS(PySDL_HasAltiVec,  SDL_HasAltiVec)
PYSDL_HAS(PySDL_HasSSE,      SDL_HasSSE)
PYSDL_HAS(PySDL_HasSSE2,     SDL_HasSSE2)
PYSDL_HAS(PySDL_HasSSE3,     SDL_HasSSE3)
PYSDL_HAS(PySDL_HasSSE41,    SDL_HasSSE41)
PYSDL_HAS(PySDL_HasSSE42,    SDL_HasSSE42)
PYSDL_HAS(PySDL_HasNEON,     SDL_HasNEON)
PYSDL_HAS(PySDL_HasLSX,      SDL_HasLSX)
PYSDL_HAS(PySDL_HasLASX,     SDL_HasLASX)

static PyObject * PySDL_GetSystemRAM(PyObject *self, PyObject *ign) {
    return PyLong_FromLong(SDL_GetSystemRAM());
}

static PyObject * PySDL_SIMDGetAlignment(PyObject *self, PyObject *ign) {
    return PyLong_FromSize_t(SDL_GetSIMDAlignment());
}

static PyObject * PySDL_GetPowerInfo(PyObject *self, PyObject *ign) {
    int seconds = -1, percent = -1;
    SDL_PowerState state = SDL_GetPowerInfo(&seconds, &percent);
    return Py_BuildValue("(iii)", state, seconds, percent);
}

//=========================================================
// displays  (SDL3: SDL_DisplayID)
//=========================================================

static PyObject * PySDL_GetNumVideoDisplays(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_DisplayID *ids = SDL_GetDisplays(&count);
    if(NULL == ids) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    SDL_free(ids);
    return PyLong_FromLong(count);
}

static PyObject * PySDL_GetDisplays(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_DisplayID *ids = SDL_GetDisplays(&count);
    if(NULL == ids) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
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

static PyObject * _mode_to_py(const SDL_DisplayMode *m) {
    return Py_BuildValue("(iiif)", m->format, m->w, m->h, m->refresh_rate);
}

static PyObject * PySDL_GetDisplayMode(PyObject *self, PyObject *args) {
    unsigned long display;
    int index = 0;
    if(!PyArg_ParseTuple(args, "k|i", &display, &index)) {
        return NULL;
    }
    int count = 0;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes((SDL_DisplayID)display, &count);
    if(NULL == modes) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    if(index < 0 || index >= count) {
        SDL_free(modes);
        PyErr_SetString(PyExc_IndexError, "display mode index out of range");
        return NULL;
    }
    PyObject *result = _mode_to_py(modes[index]);
    SDL_free(modes);
    return result;
}

static PyObject * PySDL_GetDesktopDisplayMode(PyObject *self, PyObject *arg) {
    unsigned long display = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == display && PyErr_Occurred()) {
        return NULL;
    }
    const SDL_DisplayMode *mode = SDL_GetDesktopDisplayMode((SDL_DisplayID)display);
    if(NULL == mode) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return _mode_to_py(mode);
}

static PyObject * PySDL_GetCurrentDisplayMode(PyObject *self, PyObject *arg) {
    unsigned long display = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == display && PyErr_Occurred()) {
        return NULL;
    }
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode((SDL_DisplayID)display);
    if(NULL == mode) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return _mode_to_py(mode);
}

static PyObject * PySDL_GetDisplayBounds(PyObject *self, PyObject *arg) {
    unsigned long display = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == display && PyErr_Occurred()) {
        return NULL;
    }
    SDL_Rect rect;
    if(!SDL_GetDisplayBounds((SDL_DisplayID)display, &rect)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return Py_BuildValue("(iiii)", rect.x, rect.y, rect.w, rect.h);
}

//=========================================================
// GL attributes
//=========================================================

static PyObject * PySDL_GL_SetAttribute(PyObject *self, PyObject *args) {
    int attrib, value;
    if(!PyArg_ParseTuple(args, "ii", &attrib, &value)) {
        return NULL;
    }
    if(!SDL_GL_SetAttribute((SDL_GLAttr)attrib, value)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_GL_GetAttribute(PyObject *self, PyObject *arg) {
    int attribute = PyLong_AsLong(arg);
    int value = 0;
    if(!SDL_GL_GetAttribute((SDL_GLAttr)attribute, &value)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyLong_FromLong(value);
}

static PyObject * PySDL_GL_ResetAttributes(PyObject *self, PyObject *ign) {
    SDL_GL_ResetAttributes();
    Py_RETURN_NONE;
}

static PyObject * PySDL_GL_ExtensionSupported(PyObject *self, PyObject *arg) {
    const char *extension = PyUnicode_AsUTF8(arg);
    if(NULL == extension) {
        return NULL;
    }
    return PyBool_FromLong(SDL_GL_ExtensionSupported(extension));
}

static PyObject * PySDL_GL_SetSwapInterval(PyObject *self, PyObject *arg) {
    int value = PyLong_AsLong(arg);
    if(-1 == value && PyErr_Occurred()) {
        return NULL;
    }
    if(!SDL_GL_SetSwapInterval(value)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_GL_GetSwapInterval(PyObject *self, PyObject *ign) {
    int interval = 0;
    if(!SDL_GL_GetSwapInterval(&interval)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyLong_FromLong(interval);
}

//=========================================================
// render drivers
//=========================================================

static PyObject * PySDL_GetNumRenderDrivers(PyObject *self, PyObject *args) {
    return PyLong_FromLong(SDL_GetNumRenderDrivers());
}

static PyObject * PySDL_GetRenderDriverInfo(PyObject *self, PyObject *args) {
    // SDL3 dropped SDL_RendererInfo; only the driver name is available by index.
    long index = PyLong_AsLong(args);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_GetRenderDriver((int)index);
    if(NULL == name) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyUnicode_FromString(name);
}

//=========================================================
// audio devices  (SDL3: SDL_AudioDeviceID lists)
//=========================================================

static SDL_AudioDeviceID * _audio_devices(int iscapture, int *count) {
    return iscapture ? SDL_GetAudioRecordingDevices(count)
                     : SDL_GetAudioPlaybackDevices(count);
}

static PyObject * PySDL_GetNumAudioDevices(PyObject *self, PyObject *args) {
    int isCapture = 0;
    if(!PyArg_ParseTuple(args, "|p", &isCapture)) {
        return NULL;
    }
    int count = 0;
    SDL_AudioDeviceID *ids = _audio_devices(isCapture, &count);
    SDL_free(ids);
    return PyLong_FromLong(count);
}

static PyObject * PySDL_GetAudioDeviceName(PyObject *self, PyObject *args) {
    int idx;
    int isCapture = 0;
    if(!PyArg_ParseTuple(args, "i|p", &idx, &isCapture)) {
        return NULL;
    }
    int count = 0;
    SDL_AudioDeviceID *ids = _audio_devices(isCapture, &count);
    if(NULL == ids || idx < 0 || idx >= count) {
        SDL_free(ids);
        PyErr_SetString(PyExc_IndexError, "audio device index out of range");
        return NULL;
    }
    const char *name = SDL_GetAudioDeviceName(ids[idx]);
    SDL_free(ids);
    return PyUnicode_FromString(name ? name : "");
}

//=========================================================
// timing / paths / misc
//=========================================================

static PyObject * PySDL_Delay(PyObject *self, PyObject *arg) {
    long ms = PyLong_AsLong(arg);
    if(-1 == ms && PyErr_Occurred()) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
        SDL_Delay((Uint32)ms);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyObject * PySDL_GetPerformanceCounter(PyObject *self, PyObject *ign) {
    return PyLong_FromUnsignedLongLong(SDL_GetPerformanceCounter());
}

static PyObject * PySDL_GetPerformanceFrequency(PyObject *self, PyObject *ign) {
    return PyLong_FromUnsignedLongLong(SDL_GetPerformanceFrequency());
}

static PyObject * PySDL_GetVersion(PyObject *self, PyObject *ign) {
    int v = SDL_GetVersion();
    return Py_BuildValue("(iii)",
        SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v), SDL_VERSIONNUM_MICRO(v));
}

static PyObject * PySDL_GetRevision(PyObject *self, PyObject *ign) {
    return PyUnicode_FromString(SDL_GetRevision());
}

static PyObject * PySDL_SetError(PyObject *self, PyObject *arg) {
    const char *msg = PyUnicode_AsUTF8(arg);
    if(NULL == msg) {
        return NULL;
    }
    SDL_SetError("%s", msg);
    Py_RETURN_NONE;
}

static PyObject * PySDL_ClearError(PyObject *self, PyObject *ign) {
    SDL_ClearError();
    Py_RETURN_NONE;
}

static PyObject * PySDL_GetBasePath(PyObject *self, PyObject *ign) {
    const char *path = SDL_GetBasePath();  // SDL3: owned by SDL, do not free
    if(NULL == path) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    return PyUnicode_FromString(path);
}

static PyObject * PySDL_GetPrefPath(PyObject *self, PyObject *args) {
    const char *org;
    const char *app;
    if(!PyArg_ParseTuple(args, "ss", &org, &app)) {
        return NULL;
    }
    char *path = SDL_GetPrefPath(org, app);
    if(NULL == path) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    PyObject *result = PyUnicode_FromString(path);
    SDL_free(path);
    return result;
}

static PyObject * PySDL_EnableScreenSaver(PyObject *self, PyObject *ign) {
    SDL_EnableScreenSaver();
    Py_RETURN_NONE;
}

static PyObject * PySDL_DisableScreenSaver(PyObject *self, PyObject *ign) {
    SDL_DisableScreenSaver();
    Py_RETURN_NONE;
}

static PyObject * PySDL_IsScreenSaverEnabled(PyObject *self, PyObject *ign) {
    return PyBool_FromLong(SDL_ScreenSaverEnabled());
}

static PyObject * PySDL_GetClipboardText(PyObject *self, PyObject *ign) {
    char *text = SDL_GetClipboardText();
    PyObject *result = PyUnicode_FromString(text ? text : "");
    SDL_free(text);
    return result;
}

// Setting the clipboard pushes EVENT_CLIPBOARD_UPDATE, so the GIL is dropped
// (see the GIL rule in pysdl_Events.c).
static PyObject * PySDL_SetClipboardText(PyObject *self, PyObject *arg) {
    const char *text = PyUnicode_AsUTF8(arg);
    if(NULL == text) {
        return NULL;
    }
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_SetClipboardText(text);
    Py_END_ALLOW_THREADS
    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_HasClipboardText(PyObject *self, PyObject *ign) {
    return PyBool_FromLong(SDL_HasClipboardText());
}

// The X11 / Wayland primary selection (middle-click paste). Where the video
// driver has none, SDL keeps the text itself, so it still round-trips.
static PyObject * PySDL_GetPrimarySelectionText(PyObject *self, PyObject *ign) {
    char *text = SDL_GetPrimarySelectionText();
    PyObject *result = PyUnicode_FromString(text ? text : "");
    SDL_free(text);
    return result;
}

static PyObject * PySDL_SetPrimarySelectionText(PyObject *self, PyObject *arg) {
    const char *text = PyUnicode_AsUTF8(arg);
    if(NULL == text) {
        return NULL;
    }
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_SetPrimarySelectionText(text);
    Py_END_ALLOW_THREADS
    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_HasPrimarySelectionText(PyObject *self, PyObject *ign) {
    return PyBool_FromLong(SDL_HasPrimarySelectionText());
}
