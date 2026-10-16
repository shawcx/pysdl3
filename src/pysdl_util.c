#include "pysdl.h"

int PySDL_ThreadEnter(PyGILState_STATE *state) {
    // Checked before acquiring: once finalization has run, PyGILState_Ensure
    // is no longer safe to call.
    if(Py_IsFinalizing()) {
        return 0;
    }
    *state = PyGILState_Ensure();
    return 1;
}

void PySDL_ThreadLeave(PyGILState_STATE state) {
    PyGILState_Release(state);
}

static int _seq_to_longs(PyObject *src, long *out, Py_ssize_t lo, Py_ssize_t hi, Py_ssize_t *got) {
    PyObject *fast = PySequence_Fast(src, "expected a tuple or list");
    if(NULL == fast) {
        return 0;
    }

    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    if(n < lo || n > hi) {
        PyErr_Format(PyExc_ValueError, "expected %zd to %zd items, got %zd", lo, hi, n);
        Py_DECREF(fast);
        return 0;
    }

    for(Py_ssize_t idx = 0; idx < n; ++idx) {
        out[idx] = PyLong_AsLong(PySequence_Fast_GET_ITEM(fast, idx));
        if(-1 == out[idx] && PyErr_Occurred()) {
            Py_DECREF(fast);
            return 0;
        }
    }

    Py_DECREF(fast);
    *got = n;
    return 1;
}

int PyToRect(PyObject *src, SDL_Rect *dst) {
    long v[4];
    Py_ssize_t n;

    if(!_seq_to_longs(src, v, 2, 4, &n)) {
        return 0;
    }
    if(3 == n) {
        PyErr_SetString(PyExc_ValueError, "rect takes 2 (x,y) or 4 (x,y,w,h) items");
        return 0;
    }

    dst->x = (int)v[0];
    dst->y = (int)v[1];
    if(4 == n) {
        dst->w = (int)v[2];
        dst->h = (int)v[3];
    } else {
        dst->w = -1;
        dst->h = -1;
    }
    return 1;
}

int PyToPoint(PyObject *src, SDL_Point *dst) {
    long v[2];
    Py_ssize_t n;

    if(!_seq_to_longs(src, v, 2, 2, &n)) {
        return 0;
    }
    dst->x = (int)v[0];
    dst->y = (int)v[1];
    return 1;
}

static int _seq_to_doubles(PyObject *src, double *out, Py_ssize_t lo, Py_ssize_t hi, Py_ssize_t *got) {
    PyObject *fast = PySequence_Fast(src, "expected a tuple or list");
    if(NULL == fast) {
        return 0;
    }

    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    if(n < lo || n > hi) {
        PyErr_Format(PyExc_ValueError, "expected %zd to %zd items, got %zd", lo, hi, n);
        Py_DECREF(fast);
        return 0;
    }

    for(Py_ssize_t idx = 0; idx < n; ++idx) {
        out[idx] = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(fast, idx));
        if(-1.0 == out[idx] && PyErr_Occurred()) {
            Py_DECREF(fast);
            return 0;
        }
    }

    Py_DECREF(fast);
    *got = n;
    return 1;
}

int PyToFRect(PyObject *src, SDL_FRect *dst) {
    double v[4];
    Py_ssize_t n;

    if(!_seq_to_doubles(src, v, 2, 4, &n)) {
        return 0;
    }
    if(3 == n) {
        PyErr_SetString(PyExc_ValueError, "rect takes 2 (x,y) or 4 (x,y,w,h) items");
        return 0;
    }

    dst->x = (float)v[0];
    dst->y = (float)v[1];
    if(4 == n) {
        dst->w = (float)v[2];
        dst->h = (float)v[3];
    } else {
        dst->w = -1.0f;
        dst->h = -1.0f;
    }
    return 1;
}

int PyToFPoint(PyObject *src, SDL_FPoint *dst) {
    double v[2];
    Py_ssize_t n;

    if(!_seq_to_doubles(src, v, 2, 2, &n)) {
        return 0;
    }
    dst->x = (float)v[0];
    dst->y = (float)v[1];
    return 1;
}

int PyToColor(PyObject *src, SDL_Color *dst) {
    long v[4];
    Py_ssize_t n;

    if(!_seq_to_longs(src, v, 3, 4, &n)) {
        return 0;
    }
    dst->r = (Uint8)v[0];
    dst->g = (Uint8)v[1];
    dst->b = (Uint8)v[2];
    dst->a = (4 == n) ? (Uint8)v[3] : 255;
    return 1;
}

int PyToPixel(PyObject *src, const SDL_PixelFormatDetails *format,
              const SDL_Palette *palette, Uint32 *out) {
    if(PyLong_Check(src)) {
        unsigned long value = PyLong_AsUnsignedLong(src);
        if((unsigned long)-1 == value && PyErr_Occurred()) {
            return 0;
        }
        *out = (Uint32)value;
        return 1;
    }

    SDL_Color color;
    if(!PyToColor(src, &color)) {
        return 0;
    }
    *out = SDL_MapRGBA(format, palette, color.r, color.g, color.b, color.a);
    return 1;
}

PyObject * RectToPy(const SDL_Rect *rect) {
    return Py_BuildValue("(iiii)", rect->x, rect->y, rect->w, rect->h);
}

PyObject * PointToPy(const SDL_Point *point) {
    return Py_BuildValue("(ii)", point->x, point->y);
}

PyObject * PySDL_WrapWindow(SDL_Window *window) {
    if(NULL == window) {
        Py_RETURN_NONE;
    }
    PySDL_Window *wrapper = (PySDL_Window *)PySDL_New(&PySDL_Window_Type);
    if(NULL == wrapper) {
        return NULL;
    }
    wrapper->window = window;
    wrapper->shouldFree = 0;
    return (PyObject *)wrapper;
}

//=========================================================
// file sources / save destinations  (SDL_IOStream)
//=========================================================

int PySDL_PathFromObject(PyObject *src, PyObject **path) {
    *path = NULL;
    // bytes is the file's contents here, never a path
    if(!PyUnicode_Check(src) && !PyObject_HasAttrString(src, "__fspath__")) {
        return 0;
    }
    PyObject *fspath = PyOS_FSPath(src);
    if(NULL == fspath) {
        return -1;
    }
    if(!PyUnicode_Check(fspath)) {
        Py_DECREF(fspath);
        PyErr_SetString(PyExc_TypeError, "path must be str (pass bytes for the file's contents)");
        return -1;
    }
    *path = fspath;
    return 1;
}

// Open `path` (a str) for SDL with the GIL released; raises on failure.
static SDL_IOStream * _open_file(PyObject *path, const char *mode) {
    const char *file = PyUnicode_AsUTF8(path);
    if(NULL == file) {
        return NULL;
    }
    SDL_IOStream *io;
    Py_BEGIN_ALLOW_THREADS
        io = SDL_IOFromFile(file, mode);
    Py_END_ALLOW_THREADS
    if(NULL == io) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
    }
    return io;
}

SDL_IOStream * PySDL_IOFromObject(PyObject *src, Py_buffer *view) {
    view->obj = NULL;

    PyObject *path;
    int is_path = PySDL_PathFromObject(src, &path);
    if(0 > is_path) {
        return NULL;
    }
    if(is_path) {
        SDL_IOStream *io = _open_file(path, "rb");
        Py_DECREF(path);
        return io;
    }

    if(0 > PyObject_GetBuffer(src, view, PyBUF_SIMPLE)) {
        view->obj = NULL;
        if(PyErr_ExceptionMatches(PyExc_TypeError)) {  // keep e.g. a BufferError's reason
            PyErr_Format(PyExc_TypeError, "expected a path or the file's bytes, not %.200s",
                Py_TYPE(src)->tp_name);
        }
        return NULL;
    }
    // SDL_IOFromConstMem rejects a zero size; an empty dynamic stream reads as EOF.
    SDL_IOStream *io = (0 < view->len) ? SDL_IOFromConstMem(view->buf, (size_t)view->len)
                                       : SDL_IOFromDynamicMem();
    if(NULL == io) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        PyBuffer_Release(view);  // leaves view->obj NULL
    }
    return io;
}

SDL_IOStream * PySDL_SaveOpen(PyObject *dest, PyObject **path) {
    *path = NULL;
    if(Py_None == dest) {
        SDL_IOStream *io = SDL_IOFromDynamicMem();
        if(NULL == io) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
        }
        return io;
    }

    int is_path = PySDL_PathFromObject(dest, path);
    if(0 >= is_path) {
        if(0 == is_path) {
            PyErr_Format(PyExc_TypeError, "path must be str, os.PathLike or None, not %.200s",
                Py_TYPE(dest)->tp_name);
        }
        return NULL;
    }
    SDL_IOStream *io = _open_file(*path, "wb");
    if(NULL == io) {
        Py_CLEAR(*path);
    }
    return io;
}

PyObject * PySDL_SaveClose(SDL_IOStream *io, PyObject *path, bool ok) {
    PyObject *result = NULL;

    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());  // before closing can overwrite it
        SDL_CloseIO(io);
    } else if(NULL == path) {
        // A dynamic stream's size is the high-water mark of what SDL wrote.
        Sint64 size = SDL_GetIOSize(io);
        const char *data = SDL_GetPointerProperty(SDL_GetIOProperties(io),
            SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, NULL);
        if(0 > size || (0 < size && NULL == data)) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
        } else {
            result = PyBytes_FromStringAndSize(data, (Py_ssize_t)size);
        }
        SDL_CloseIO(io);  // frees the memory
    } else {
        bool closed;
        Py_BEGIN_ALLOW_THREADS
            closed = SDL_CloseIO(io);  // flushes the file
        Py_END_ALLOW_THREADS
        if(!closed) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
        } else {
            Py_INCREF(Py_None);
            result = Py_None;
        }
    }

    Py_XDECREF(path);
    return result;
}

//=========================================================
// wrapper registries
//=========================================================

int PySDL_RegistryAdd(PySDL_Registry *reg, PyObject *obj) {
    for(Py_ssize_t idx = 0; idx < reg->len; ++idx) {
        if(reg->items[idx] == obj) {
            return 0;
        }
    }
    if(reg->len == reg->cap) {
        Py_ssize_t cap = reg->cap ? reg->cap * 2 : 16;
        PyObject **grown = PyMem_Realloc(reg->items, cap * sizeof(PyObject *));
        if(NULL == grown) {
            PyErr_NoMemory();
            return -1;
        }
        reg->items = grown;
        reg->cap = cap;
    }
    reg->items[reg->len++] = obj;
    return 0;
}

void PySDL_RegistryRemove(PySDL_Registry *reg, PyObject *obj) {
    for(Py_ssize_t idx = 0; idx < reg->len; ++idx) {
        if(reg->items[idx] == obj) {
            reg->items[idx] = reg->items[--reg->len];  // order doesn't matter
            return;
        }
    }
}
