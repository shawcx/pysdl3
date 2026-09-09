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
