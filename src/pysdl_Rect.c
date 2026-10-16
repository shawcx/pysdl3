#include "pysdl.h"

// Pure geometry helpers over the same tuple/list rects and points the rest of
// the binding accepts. Registered via PyModule_AddFunctions.

// A line `(x1, y1, x2, y2)` (tuple or list) parsed with `fmt` ("iiii" /
// "ffff"); 0 with an exception set on bad input.
static int _parse_line(PyObject *line_py, const char *fmt, void *x1, void *y1, void *x2, void *y2) {
    PyObject *line = PySequence_Tuple(line_py);
    if(NULL == line) {
        return 0;
    }
    int ok = PyArg_ParseTuple(line, fmt, x1, y1, x2, y2);
    Py_DECREF(line);
    return ok;
}

//=========================================================
// integer rects / points
//=========================================================

static PyObject * PySDL_HasIntersection(PyObject *self, PyObject *args) {
    PyObject *a_py, *b_py;
    SDL_Rect a, b;
    if(!PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !PyToRect(a_py, &a) || !PyToRect(b_py, &b)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_HasRectIntersection(&a, &b));
}

static PyObject * PySDL_IntersectRect(PyObject *self, PyObject *args) {
    PyObject *a_py, *b_py;
    SDL_Rect a, b, result;
    if(!PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !PyToRect(a_py, &a) || !PyToRect(b_py, &b)) {
        return NULL;
    }
    if(false == SDL_GetRectIntersection(&a, &b, &result)) {
        Py_RETURN_NONE;
    }
    return RectToPy(&result);
}

static PyObject * PySDL_UnionRect(PyObject *self, PyObject *args) {
    PyObject *a_py, *b_py;
    SDL_Rect a, b, result;
    if(!PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !PyToRect(a_py, &a) || !PyToRect(b_py, &b)) {
        return NULL;
    }
    SDL_GetRectUnion(&a, &b, &result);
    return RectToPy(&result);
}

static PyObject * PySDL_EnclosePoints(PyObject *self, PyObject *args) {
    PyObject *points_py;
    PyObject *clip_py = Py_None;
    if(!PyArg_ParseTuple(args, "O|O", &points_py, &clip_py)) {
        return NULL;
    }

    SDL_Rect clip;
    SDL_Rect *clipp = NULL;
    if(clip_py != Py_None) {
        if(!PyToRect(clip_py, &clip)) {
            return NULL;
        }
        clipp = &clip;
    }

    PyObject *fast = PySequence_Fast(points_py, "expected a list of (x, y) points");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    if(0 == n || n > INT_MAX) {
        Py_DECREF(fast);
        if(0 == n) {
            Py_RETURN_NONE;  // nothing to enclose (SDL would set an error)
        }
        PyErr_SetString(PyExc_ValueError, "too many points");
        return NULL;
    }
    SDL_Point *points = PyMem_New(SDL_Point, n);
    if(NULL == points) {
        Py_DECREF(fast);
        return PyErr_NoMemory();
    }
    for(Py_ssize_t idx = 0; idx < n; ++idx) {
        if(!PyToPoint(PySequence_Fast_GET_ITEM(fast, idx), &points[idx])) {
            PyMem_Free(points);
            Py_DECREF(fast);
            return NULL;
        }
    }
    Py_DECREF(fast);

    SDL_Rect result;
    bool ok = SDL_GetRectEnclosingPoints(points, (int)n, clipp, &result);
    PyMem_Free(points);
    if(false == ok) {
        Py_RETURN_NONE;
    }
    return RectToPy(&result);
}

static PyObject * PySDL_IntersectRectAndLine(PyObject *self, PyObject *args) {
    PyObject *rect_py, *line_py;
    SDL_Rect rect;
    int x1, y1, x2, y2;
    if(!PyArg_ParseTuple(args, "OO", &rect_py, &line_py)
        || !PyToRect(rect_py, &rect)
        || !_parse_line(line_py, "iiii", &x1, &y1, &x2, &y2)) {
        return NULL;
    }
    if(false == SDL_GetRectAndLineIntersection(&rect, &x1, &y1, &x2, &y2)) {
        Py_RETURN_NONE;
    }
    return Py_BuildValue("(iiii)", x1, y1, x2, y2);
}

static PyObject * PySDL_PointInRect(PyObject *self, PyObject *args) {
    PyObject *point_py, *rect_py;
    SDL_Point point;
    SDL_Rect rect;
    if(!PyArg_ParseTuple(args, "OO", &point_py, &rect_py)
        || !PyToPoint(point_py, &point) || !PyToRect(rect_py, &rect)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_PointInRect(&point, &rect));
}

static PyObject * PySDL_RectEmpty(PyObject *self, PyObject *arg) {
    SDL_Rect rect;
    if(!PyToRect(arg, &rect)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_RectEmpty(&rect));
}

static PyObject * PySDL_RectEquals(PyObject *self, PyObject *args) {
    PyObject *a_py, *b_py;
    SDL_Rect a, b;
    if(!PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !PyToRect(a_py, &a) || !PyToRect(b_py, &b)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_RectsEqual(&a, &b));
}

//=========================================================
// float rects / points
//=========================================================

static PyObject * _frect_to_py(const SDL_FRect *rect) {
    return Py_BuildValue("(ffff)", rect->x, rect->y, rect->w, rect->h);
}

static PyObject * PySDL_HasIntersectionF(PyObject *self, PyObject *args) {
    PyObject *a_py, *b_py;
    SDL_FRect a, b;
    if(!PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !PyToFRect(a_py, &a) || !PyToFRect(b_py, &b)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_HasRectIntersectionFloat(&a, &b));
}

static PyObject * PySDL_IntersectFRect(PyObject *self, PyObject *args) {
    PyObject *a_py, *b_py;
    SDL_FRect a, b, result;
    if(!PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !PyToFRect(a_py, &a) || !PyToFRect(b_py, &b)) {
        return NULL;
    }
    if(false == SDL_GetRectIntersectionFloat(&a, &b, &result)) {
        Py_RETURN_NONE;
    }
    return _frect_to_py(&result);
}

static PyObject * PySDL_UnionFRect(PyObject *self, PyObject *args) {
    PyObject *a_py, *b_py;
    SDL_FRect a, b, result;
    if(!PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !PyToFRect(a_py, &a) || !PyToFRect(b_py, &b)) {
        return NULL;
    }
    SDL_GetRectUnionFloat(&a, &b, &result);
    return _frect_to_py(&result);
}

static PyObject * PySDL_PointInFRect(PyObject *self, PyObject *args) {
    PyObject *point_py, *rect_py;
    SDL_FPoint point;
    SDL_FRect rect;
    if(!PyArg_ParseTuple(args, "OO", &point_py, &rect_py)
        || !PyToFPoint(point_py, &point) || !PyToFRect(rect_py, &rect)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_PointInRectFloat(&point, &rect));
}

static PyObject * PySDL_FRectEmpty(PyObject *self, PyObject *arg) {
    SDL_FRect rect;
    if(!PyToFRect(arg, &rect)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_RectEmptyFloat(&rect));
}

static PyObject * PySDL_FRectEquals(PyObject *self, PyObject *args) {
    PyObject *a_py, *b_py;
    SDL_FRect a, b;
    if(!PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !PyToFRect(a_py, &a) || !PyToFRect(b_py, &b)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_RectsEqualFloat(&a, &b));
}

// EncloseFPoints(points, clip=None) -> the smallest float rect holding the
// points (only those inside clip, if given), or None if none qualify. Unlike
// the integer version (and SDL2's float one), SDL3 adds no +1: the rect spans
// exactly min..max, and a clip's far edge x + w is inside.
static PyObject * PySDL_EncloseFPoints(PyObject *self, PyObject *args) {
    PyObject *points_py;
    PyObject *clip_py = Py_None;
    if(!PyArg_ParseTuple(args, "O|O", &points_py, &clip_py)) {
        return NULL;
    }

    SDL_FRect clip;
    SDL_FRect *clipp = NULL;
    if(clip_py != Py_None) {
        if(!PyToFRect(clip_py, &clip)) {
            return NULL;
        }
        clipp = &clip;
    }

    PyObject *fast = PySequence_Fast(points_py, "expected a list of (x, y) points");
    if(NULL == fast) {
        return NULL;
    }
    Py_ssize_t n = PySequence_Fast_GET_SIZE(fast);
    if(0 == n || n > INT_MAX) {
        Py_DECREF(fast);
        if(0 == n) {
            Py_RETURN_NONE;
        }
        PyErr_SetString(PyExc_ValueError, "too many points");
        return NULL;
    }
    SDL_FPoint *points = PyMem_New(SDL_FPoint, n);
    if(NULL == points) {
        Py_DECREF(fast);
        return PyErr_NoMemory();
    }
    for(Py_ssize_t idx = 0; idx < n; ++idx) {
        if(!PyToFPoint(PySequence_Fast_GET_ITEM(fast, idx), &points[idx])) {
            PyMem_Free(points);
            Py_DECREF(fast);
            return NULL;
        }
    }
    Py_DECREF(fast);

    SDL_FRect result;
    bool ok = SDL_GetRectEnclosingPointsFloat(points, (int)n, clipp, &result);
    PyMem_Free(points);
    if(!ok) {
        Py_RETURN_NONE;
    }
    return _frect_to_py(&result);
}

// IntersectFRectAndLine(frect, (x1, y1, x2, y2)) -> the clipped line, or None.
// The rect's far edges (x + w, y + h) count as inside.
static PyObject * PySDL_IntersectFRectAndLine(PyObject *self, PyObject *args) {
    PyObject *rect_py, *line_py;
    SDL_FRect rect;
    float x1, y1, x2, y2;
    if(!PyArg_ParseTuple(args, "OO", &rect_py, &line_py)
        || !PyToFRect(rect_py, &rect)
        || !_parse_line(line_py, "ffff", &x1, &y1, &x2, &y2)) {
        return NULL;
    }
    if(!SDL_GetRectAndLineIntersectionFloat(&rect, &x1, &y1, &x2, &y2)) {
        Py_RETURN_NONE;
    }
    return Py_BuildValue("(ffff)", x1, y1, x2, y2);
}

PyMethodDef pysdl_rect_methods[] = {
    { "HasIntersection",      PySDL_HasIntersection,      METH_VARARGS },
    { "IntersectRect",        PySDL_IntersectRect,        METH_VARARGS },
    { "UnionRect",            PySDL_UnionRect,            METH_VARARGS },
    { "EnclosePoints",        PySDL_EnclosePoints,        METH_VARARGS },
    { "IntersectRectAndLine", PySDL_IntersectRectAndLine, METH_VARARGS },
    { "PointInRect",          PySDL_PointInRect,          METH_VARARGS },
    { "RectEmpty",            PySDL_RectEmpty,            METH_O       },
    { "RectEquals",           PySDL_RectEquals,           METH_VARARGS },
    { "HasIntersectionF",     PySDL_HasIntersectionF,     METH_VARARGS },
    { "IntersectFRect",       PySDL_IntersectFRect,       METH_VARARGS },
    { "UnionFRect",           PySDL_UnionFRect,           METH_VARARGS },
    { "PointInFRect",         PySDL_PointInFRect,         METH_VARARGS },
    { "FRectEmpty",           PySDL_FRectEmpty,           METH_O       },
    { "FRectEquals",          PySDL_FRectEquals,          METH_VARARGS },
    { "EncloseFPoints",       PySDL_EncloseFPoints,       METH_VARARGS },
    { "IntersectFRectAndLine", PySDL_IntersectFRectAndLine, METH_VARARGS },
    { NULL }
};
