#include "pysdl.h"

// SDL_image: LoadImage (optionally typed), the IMG_is* format probes, sized
// SVG, XPM-from-array, animations and the library version. Sources are a path
// (str / os.PathLike) or the file's bytes, per PySDL_IOFromObject. New module
// functions keep the IMG_ prefix; LoadImage predates that. Saving lives on
// Surface (SavePNG / SaveJPG / ...), texture loading on Renderer.LoadTexture
// (through PySDL_IMG_LoadTexture below). SDL3_image has no IMG_Init / IMG_Quit.
// Registered in PyInit_SDL3 via PyModule_AddFunctions(module, pysdl_image_methods).

static PyObject * PySDL_IMG_Version                (PyObject*, PyObject*);
static PyObject * PySDL_LoadImage                  (PyObject*, PyObject*, PyObject*);
static PyObject * PySDL_IMG_LoadSizedSVG           (PyObject*, PyObject*);
static PyObject * PySDL_IMG_ReadXPMFromArray       (PyObject*, PyObject*);
static PyObject * PySDL_IMG_ReadXPMFromArrayToRGB888 (PyObject*, PyObject*);
static PyObject * PySDL_IMG_LoadAnimation          (PyObject*, PyObject*, PyObject*);
#if SDL_IMAGE_VERSION_ATLEAST(3,4,0)
static PyObject * PySDL_IMG_SaveAnimation          (PyObject*, PyObject*, PyObject*);
static PyObject * PySDL_IMG_GetClipboardImage      (PyObject*, PyObject*);
#endif

// format probes: IMG_is<FMT>(src) -> bool
#define IMG_IS_PROTO(FMT) static PyObject * PySDL_IMG_is##FMT(PyObject*, PyObject*);
#define IMG_IS_ENTRY(FMT) { "IMG_is" #FMT, PySDL_IMG_is##FMT, METH_O },
#define IMG_IS_FORMATS(X) \
    X(AVIF) X(BMP) X(CUR) X(GIF) X(ICO) X(JPG) X(JXL) X(LBM) X(PCX) \
    X(PNG) X(PNM) X(QOI) X(SVG) X(TIF) X(WEBP) X(XCF) X(XPM) X(XV)

IMG_IS_FORMATS(IMG_IS_PROTO)
#if SDL_IMAGE_VERSION_ATLEAST(3,4,0)
IMG_IS_PROTO(ANI)
#endif

PyMethodDef pysdl_image_methods[] = {
    { "IMG_Version",          PySDL_IMG_Version,          METH_NOARGS  },
    { "IMG_Linked_Version",   PySDL_IMG_Version,          METH_NOARGS  },  // pysdl2 name
    { "LoadImage",            (PyCFunction)PySDL_LoadImage, METH_VARARGS | METH_KEYWORDS },
    { "IMG_LoadSizedSVG",     PySDL_IMG_LoadSizedSVG,     METH_VARARGS },
    { "IMG_ReadXPMFromArray", PySDL_IMG_ReadXPMFromArray, METH_O       },
    { "IMG_ReadXPMFromArrayToRGB888", PySDL_IMG_ReadXPMFromArrayToRGB888, METH_O },
    { "IMG_LoadAnimation",    (PyCFunction)PySDL_IMG_LoadAnimation, METH_VARARGS | METH_KEYWORDS },
#if SDL_IMAGE_VERSION_ATLEAST(3,4,0)
    { "IMG_SaveAnimation",    (PyCFunction)PySDL_IMG_SaveAnimation, METH_VARARGS | METH_KEYWORDS },
    { "IMG_GetClipboardImage", PySDL_IMG_GetClipboardImage, METH_NOARGS },
#endif
    IMG_IS_FORMATS(IMG_IS_ENTRY)
#if SDL_IMAGE_VERSION_ATLEAST(3,4,0)
    IMG_IS_ENTRY(ANI)
#endif
    { NULL }
};

static PyObject * _raise(void) {
    PyErr_SetString(pysdl_Error, SDL_GetError());
    return NULL;
}

// Wrap a surface SDL_image allocated as an owned SDL3.Surface; frees it on failure.
static PyObject * _wrap_surface(SDL_Surface *surface) {
    if(NULL == surface) {
        return _raise();
    }
    PySDL_Surface *wrapper = (PySDL_Surface *)PySDL_New(&PySDL_Surface_Type);
    if(NULL == wrapper) {
        SDL_DestroySurface(surface);
        return NULL;
    }
    wrapper->surface = surface;
    return (PyObject *)wrapper;
}

//=========================================================
// version
//=========================================================

// -> (major, minor, micro) of the SDL_image library actually loaded
static PyObject * PySDL_IMG_Version(PyObject *self, PyObject *ign) {
    int v = IMG_Version();
    return Py_BuildValue("(iii)",
        SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v), SDL_VERSIONNUM_MICRO(v));
}

//=========================================================
// loading
//=========================================================

enum { LOAD_SURFACE, LOAD_ANIMATION, LOAD_TEXTURE };

// Load `src` (a path or the file's bytes) as a surface, an animation or a
// texture for `renderer`. Formats with a magic number are always detected from
// the data; `type` ("TGA") only matters for magic-less ones. A path with no
// type goes through the IMG_Load*(file) call so its extension can still name
// the format. SDL_image reads everything during the call (closing the stream),
// so a bytes source is pinned only that long. The GIL is released for the
// open and the decode. NULL with an exception set on failure.
static void * _load(int kind, SDL_Renderer *renderer, PyObject *src, const char *type) {
    void *result = NULL;
    PyObject *path = NULL;
    int is_path = type ? 0 : PySDL_PathFromObject(src, &path);
    if(0 > is_path) {
        return NULL;
    }

    if(is_path) {
        const char *file = PyUnicode_AsUTF8(path);
        if(NULL == file) {
            Py_DECREF(path);
            return NULL;
        }
        Py_BEGIN_ALLOW_THREADS
            switch(kind) {
            case LOAD_SURFACE:   result = IMG_Load(file);                    break;
            case LOAD_ANIMATION: result = IMG_LoadAnimation(file);           break;
            default:             result = IMG_LoadTexture(renderer, file);   break;
            }
        Py_END_ALLOW_THREADS
        Py_DECREF(path);
    } else {
        Py_buffer view;
        SDL_IOStream *io = PySDL_IOFromObject(src, &view);
        if(NULL == io) {
            return NULL;
        }
        Py_BEGIN_ALLOW_THREADS
            switch(kind) {
            case LOAD_SURFACE:   result = IMG_LoadTyped_IO(io, true, type);                    break;
            case LOAD_ANIMATION: result = IMG_LoadAnimationTyped_IO(io, true, type);           break;
            default:             result = IMG_LoadTextureTyped_IO(renderer, io, true, type);   break;
            }
        Py_END_ALLOW_THREADS
        PyBuffer_Release(&view);
    }

    if(NULL == result) {
        _raise();
    }
    return result;
}

SDL_Texture * PySDL_IMG_LoadTexture(SDL_Renderer *renderer, PyObject *src, const char *type) {
    return _load(LOAD_TEXTURE, renderer, src, type);
}

// LoadImage(src, type=None) -> Surface. That `type` pass-through makes this
// equivalent to the per-format IMG_Load<FMT>_IO functions.
static PyObject * PySDL_LoadImage(PyObject *self, PyObject *args, PyObject *kwds) {
    PyObject *src;
    const char *type = NULL;
    static char *kwlist[] = {"src", "type", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|z", kwlist, &src, &type)) {
        return NULL;
    }
    SDL_Surface *surface = _load(LOAD_SURFACE, NULL, src, type);
    return surface ? _wrap_surface(surface) : NULL;
}

// IMG_LoadSizedSVG(src, width, height) -> Surface rasterised at that size (a 0
// dimension keeps the aspect ratio).
static PyObject * PySDL_IMG_LoadSizedSVG(PyObject *self, PyObject *args) {
    PyObject *src;
    int width, height;
    if(!PyArg_ParseTuple(args, "Oii", &src, &width, &height)) {
        return NULL;
    }
    Py_buffer view;
    SDL_IOStream *io = PySDL_IOFromObject(src, &view);
    if(NULL == io) {
        return NULL;
    }
    SDL_Surface *surface;
    Py_BEGIN_ALLOW_THREADS
        surface = IMG_LoadSizedSVG_IO(io, width, height);  // takes no closeio
        SDL_CloseIO(io);
    Py_END_ALLOW_THREADS
    PyBuffer_Release(&view);
    return _wrap_surface(surface);
}

//=========================================================
// XPM from an array of lines
//=========================================================

// SDL_image reads an XPM array with no bounds checks: a colour line shorter
// than chars_per_pixel + 1 or a pixel row shorter than width * chars_per_pixel
// is read past its end, missing lines give a half-read image with no error,
// and (before 3.4.4) a one-char pixel with no colour dereferences NULL. So
// check the lines against the header first. The header's four numbers are
// parsed strictly (unsigned, at most 9 digits) so SDL's own sscanf cannot
// read other values than these.
static int _xpm_check(char **lines, const Py_ssize_t *sizes, Py_ssize_t count) {
    Py_ssize_t value[4];  // width, height, ncolors, chars_per_pixel
    const char *p = lines[0];
    for(int idx = 0; idx < 4; ++idx) {
        while(' ' == *p || ('\t' <= *p && *p <= '\r')) {
            ++p;
        }
        int digits = 0;
        value[idx] = 0;
        while('0' <= *p && *p <= '9' && ++digits <= 9) {
            value[idx] = value[idx] * 10 + (*p++ - '0');
        }
        if(0 == digits || 9 < digits || 0 == value[idx]) {
            PyErr_SetString(PyExc_ValueError,
                "XPM header must start with \"<width> <height> <ncolors> <chars_per_pixel>\"");
            return 0;
        }
    }
    Py_ssize_t width = value[0], height = value[1], ncolors = value[2], cpp = value[3];

    if(count < 1 + ncolors + height) {
        PyErr_Format(PyExc_ValueError, "XPM header promises %zd colours and %zd rows, got %zd lines",
            ncolors, height, count);
        return 0;
    }
    for(Py_ssize_t idx = 1; idx <= ncolors; ++idx) {
        if(sizes[idx] < cpp + 1) {
            PyErr_Format(PyExc_ValueError, "XPM colour line %zd is too short", idx);
            return 0;
        }
    }
    if(cpp > PY_SSIZE_T_MAX / width) {
        PyErr_SetString(PyExc_ValueError, "XPM width * chars_per_pixel is too large");
        return 0;
    }
    char defined[256] = {0};  // one-char colour keys
    if(1 == cpp) {
        for(Py_ssize_t idx = 1; idx <= ncolors; ++idx) {
            defined[(unsigned char)lines[idx][0]] = 1;
        }
    }
    for(Py_ssize_t row = 1 + ncolors; row < 1 + ncolors + height; ++row) {
        if(sizes[row] < width * cpp) {
            PyErr_Format(PyExc_ValueError, "XPM pixel row %zd is shorter than width * chars_per_pixel",
                row - 1 - ncolors);
            return 0;
        }
        for(Py_ssize_t x = 0; 1 == cpp && x < width; ++x) {
            if(!defined[(unsigned char)lines[row][x]]) {
                PyErr_Format(PyExc_ValueError, "XPM pixel row %zd uses an undefined colour",
                    row - 1 - ncolors);
                return 0;
            }
        }
    }
    return 1;
}

static PyObject * _read_xpm(PyObject *arg, int rgb888) {
    PyObject *fast = PySequence_Fast(arg, "expected a list of XPM lines (str)");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(fast);
    if(0 == count) {
        Py_DECREF(fast);
        PyErr_SetString(PyExc_ValueError, "empty XPM");
        return NULL;
    }

    // NULL-terminated, as SDL_image expects; the pointers borrow from `fast`.
    char **lines = PyMem_Calloc(count + 1, sizeof(char *));
    Py_ssize_t *sizes = PyMem_Calloc(count, sizeof(Py_ssize_t));
    if(NULL == lines || NULL == sizes) {
        PyMem_Free(lines);
        PyMem_Free(sizes);
        Py_DECREF(fast);
        return PyErr_NoMemory();
    }

    PyObject *result = NULL;
    for(Py_ssize_t idx = 0; idx < count; ++idx) {
        PyObject *item = PySequence_Fast_GET_ITEM(fast, idx);
        if(!PyUnicode_Check(item)) {
            PyErr_Format(PyExc_TypeError, "XPM lines must be str, not %.200s", Py_TYPE(item)->tp_name);
            goto done;
        }
        lines[idx] = (char *)PyUnicode_AsUTF8AndSize(item, &sizes[idx]);
        if(NULL == lines[idx]) {
            goto done;
        }
    }
    if(_xpm_check(lines, sizes, count)) {
        result = _wrap_surface(rgb888 ? IMG_ReadXPMFromArrayToRGB888(lines) : IMG_ReadXPMFromArray(lines));
    }

done:
    PyMem_Free(lines);
    PyMem_Free(sizes);
    Py_DECREF(fast);
    return result;
}

// IMG_ReadXPMFromArray(lines) -> Surface (8-bit paletted, or 32-bit for > 256 colours)
static PyObject * PySDL_IMG_ReadXPMFromArray(PyObject *self, PyObject *arg) {
    return _read_xpm(arg, 0);
}

// IMG_ReadXPMFromArrayToRGB888(lines) -> Surface (always 32-bit)
static PyObject * PySDL_IMG_ReadXPMFromArrayToRGB888(PyObject *self, PyObject *arg) {
    return _read_xpm(arg, 1);
}

//=========================================================
// animation
//=========================================================

// IMG_LoadAnimation(src, type=None) -> (w, h, [(Surface, delay_ms), ...])
// Each frame becomes an owned Surface (taken out of the IMG_Animation before
// IMG_FreeAnimation runs), so there is no separate animation type to free.
// GIF / WEBP (and on SDL_image 3.4 APNG / ANI / AVIF) are detected from the
// data, which covers the per-format IMG_Load<FMT>Animation_IO functions; a
// still image comes back as one frame.
static PyObject * PySDL_IMG_LoadAnimation(PyObject *self, PyObject *args, PyObject *kwds) {
    PyObject *src;
    const char *type = NULL;
    static char *kwlist[] = {"src", "type", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|z", kwlist, &src, &type)) {
        return NULL;
    }
    IMG_Animation *anim = _load(LOAD_ANIMATION, NULL, src, type);
    if(NULL == anim) {
        return NULL;
    }

    PyObject *frames = PyList_New(anim->count);
    if(NULL == frames) {
        IMG_FreeAnimation(anim);
        return NULL;
    }
    for(int idx = 0; idx < anim->count; ++idx) {
        PySDL_Surface *wrapper = (PySDL_Surface *)PySDL_New(&PySDL_Surface_Type);
        if(NULL == wrapper) {
            Py_DECREF(frames);
            IMG_FreeAnimation(anim);
            return NULL;
        }
        wrapper->surface = anim->frames[idx];
        anim->frames[idx] = NULL;  // now owned by the wrapper
        PyObject *item = Py_BuildValue("(Ni)", (PyObject *)wrapper, anim->delays[idx]);
        if(NULL == item) {
            Py_DECREF(frames);
            IMG_FreeAnimation(anim);
            return NULL;
        }
        PyList_SET_ITEM(frames, idx, item);
    }

    PyObject *result = Py_BuildValue("(iiN)", anim->w, anim->h, frames);
    IMG_FreeAnimation(anim);  // frees the (now NULL) frame slots and the arrays
    return result;
}

#if SDL_IMAGE_VERSION_ATLEAST(3,4,0)
// IMG_SaveAnimation(frames, path=None, type=None) -> None, or bytes with no path
// `frames` is [(Surface, delay_ms), ...] as IMG_LoadAnimation returns it, all
// the same size. `type` ("GIF", "WEBP", "APNG", "AVIF", "ANI") defaults to the
// path's extension; WEBP / AVIF use quality 90.
static PyObject * PySDL_IMG_SaveAnimation(PyObject *self, PyObject *args, PyObject *kwds) {
    PyObject *frames_py;
    PyObject *dest = Py_None;
    const char *type = NULL;
    static char *kwlist[] = {"frames", "path", "type", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|Oz", kwlist, &frames_py, &dest, &type)) {
        return NULL;
    }
    if(NULL == type && Py_None == dest) {
        PyErr_SetString(PyExc_TypeError, "IMG_SaveAnimation() needs a type when no path is given");
        return NULL;
    }

    PyObject *fast = PySequence_Fast(frames_py, "expected a list of (Surface, delay_ms) frames");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t count = PySequence_Fast_GET_SIZE(fast);
    if(0 == count || INT_MAX < count) {
        Py_DECREF(fast);
        PyErr_SetString(PyExc_ValueError, "an animation needs at least one frame");
        return NULL;
    }

    // The frames are borrowed from the wrappers `fast` keeps alive; the GIL is
    // held throughout, so nothing can free them under SDL.
    IMG_Animation anim = { .count = (int)count };
    anim.frames = PyMem_Calloc(count, sizeof(SDL_Surface *));
    anim.delays = PyMem_Calloc(count, sizeof(int));
    PyObject *result = NULL;
    if(NULL == anim.frames || NULL == anim.delays) {
        PyErr_NoMemory();
        goto done;
    }
    for(Py_ssize_t idx = 0; idx < count; ++idx) {
        PyObject *frame = PySequence_Fast_GET_ITEM(fast, idx);
        PySDL_Surface *surface;
        int delay;
        if(!PyTuple_Check(frame) || !PyArg_ParseTuple(frame, "O!i", &PySDL_Surface_Type, &surface, &delay)) {
            PyErr_Format(PyExc_TypeError, "frame %zd must be a (Surface, delay_ms) tuple", idx);
            goto done;
        }
        SDL_Surface *s = surface->surface;
        if(NULL == s || 0 >= s->w || 0 >= s->h) {
            PyErr_Format(PyExc_ValueError, "frame %zd is an empty surface", idx);
            goto done;
        }
        if(SDL_ISPIXELFORMAT_INDEXED(s->format) && NULL == SDL_GetSurfacePalette(s)) {
            PyErr_Format(pysdl_Error, "frame %zd: indexed surfaces must have a palette", idx);
            goto done;
        }
        if(0 == idx) {
            anim.w = s->w;
            anim.h = s->h;
        } else if(s->w != anim.w || s->h != anim.h) {
            PyErr_Format(PyExc_ValueError, "frame %zd is %dx%d, the first is %dx%d",
                idx, s->w, s->h, anim.w, anim.h);
            goto done;
        }
        if(0 > delay) {
            PyErr_Format(PyExc_ValueError, "frame %zd has a negative delay", idx);
            goto done;
        }
        anim.frames[idx] = s;
        anim.delays[idx] = delay;
    }

    if(NULL == type) {
        // Same as IMG_SaveAnimation(file): the type is the path's extension.
        PyObject *path;
        int is_path = PySDL_PathFromObject(dest, &path);
        if(0 >= is_path) {
            if(0 == is_path) {
                PyErr_SetString(PyExc_TypeError, "path must be str, os.PathLike or None");
            }
            goto done;
        }
        const char *file = PyUnicode_AsUTF8(path);
        if(NULL != file) {
            if(IMG_SaveAnimation(&anim, file)) {
                Py_INCREF(Py_None);
                result = Py_None;
            } else {
                _raise();
            }
        }
        Py_DECREF(path);
    } else {
        PyObject *path;
        SDL_IOStream *io = PySDL_SaveOpen(dest, &path);
        if(NULL != io) {
            result = PySDL_SaveClose(io, path, IMG_SaveAnimationTyped_IO(&anim, io, false, type));
        }
    }

done:
    PyMem_Free(anim.frames);
    PyMem_Free(anim.delays);
    Py_DECREF(fast);
    return result;
}

// IMG_GetClipboardImage() -> Surface, or None when the clipboard holds no image
static PyObject * PySDL_IMG_GetClipboardImage(PyObject *self, PyObject *ign) {
    SDL_Surface *surface = IMG_GetClipboardImage();
    if(NULL == surface) {
        Py_RETURN_NONE;
    }
    return _wrap_surface(surface);
}
#endif

//=========================================================
// format probes: IMG_is<FMT>(src) -> bool
//=========================================================

// Each probe reads a few bytes and seeks back; it never raises for "not this
// format" (or an empty source), only for a bad argument or unopenable path.
static PyObject * _is(PyObject *src, bool (SDLCALL *probe)(SDL_IOStream *)) {
    Py_buffer view;
    SDL_IOStream *io = PySDL_IOFromObject(src, &view);
    if(NULL == io) {
        return NULL;
    }
    bool yes;
    Py_BEGIN_ALLOW_THREADS
        yes = probe(io);
        SDL_CloseIO(io);
    Py_END_ALLOW_THREADS
    PyBuffer_Release(&view);
    return PyBool_FromLong(yes);
}

#define IMG_IS_IMPL(FMT) \
    static PyObject * PySDL_IMG_is##FMT(PyObject *self, PyObject *src) { \
        return _is(src, IMG_is##FMT); \
    }

IMG_IS_FORMATS(IMG_IS_IMPL)
#if SDL_IMAGE_VERSION_ATLEAST(3,4,0)
IMG_IS_IMPL(ANI)
#endif
