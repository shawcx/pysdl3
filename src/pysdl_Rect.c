#include "pysdl.h"

// Pure geometry helpers over the same tuple/list rects and points the rest of
// the binding accepts. Registered via PyModule_AddFunctions.

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
    return PyBool_FromLong(SDL_HasIntersection(&a, &b));
}

static PyObject * PySDL_IntersectRect(PyObject *self, PyObject *args) {
    PyObject *a_py, *b_py;
    SDL_Rect a, b, result;
    if(!PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !PyToRect(a_py, &a) || !PyToRect(b_py, &b)) {
        return NULL;
    }
    if(false == SDL_IntersectRect(&a, &b, &result)) {
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
    SDL_UnionRect(&a, &b, &result);
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
    SDL_Point *points = PyMem_New(SDL_Point, n > 0 ? n : 1);
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
    bool ok = SDL_EnclosePoints(points, (int)n, clipp, &result);
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
        || !PyArg_ParseTuple(line_py, "iiii", &x1, &y1, &x2, &y2)) {
        return NULL;
    }
    if(false == SDL_IntersectRectAndLine(&rect, &x1, &y1, &x2, &y2)) {
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
    return PyBool_FromLong(SDL_RectEquals(&a, &b));
}

//=========================================================
// float rects / points (SDL >= 2.0.22)
//=========================================================

#if SDL_VERSION_ATLEAST(2,0,22)
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
    return PyBool_FromLong(SDL_HasIntersectionF(&a, &b));
}

static PyObject * PySDL_IntersectFRect(PyObject *self, PyObject *args) {
    PyObject *a_py, *b_py;
    SDL_FRect a, b, result;
    if(!PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !PyToFRect(a_py, &a) || !PyToFRect(b_py, &b)) {
        return NULL;
    }
    if(false == SDL_IntersectFRect(&a, &b, &result)) {
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
    SDL_UnionFRect(&a, &b, &result);
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
    return PyBool_FromLong(SDL_PointInFRect(&point, &rect));
}

static PyObject * PySDL_FRectEmpty(PyObject *self, PyObject *arg) {
    SDL_FRect rect;
    if(!PyToFRect(arg, &rect)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_FRectEmpty(&rect));
}

static PyObject * PySDL_FRectEquals(PyObject *self, PyObject *args) {
    PyObject *a_py, *b_py;
    SDL_FRect a, b;
    if(!PyArg_ParseTuple(args, "OO", &a_py, &b_py)
        || !PyToFRect(a_py, &a) || !PyToFRect(b_py, &b)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_FRectEquals(&a, &b));
}
#endif

PyMethodDef pysdl_rect_methods[] = {
    { "HasIntersection",      PySDL_HasIntersection,      METH_VARARGS },
    { "IntersectRect",        PySDL_IntersectRect,        METH_VARARGS },
    { "UnionRect",            PySDL_UnionRect,            METH_VARARGS },
    { "EnclosePoints",        PySDL_EnclosePoints,        METH_VARARGS },
    { "IntersectRectAndLine", PySDL_IntersectRectAndLine, METH_VARARGS },
    { "PointInRect",          PySDL_PointInRect,          METH_VARARGS },
    { "RectEmpty",            PySDL_RectEmpty,            METH_O       },
    { "RectEquals",           PySDL_RectEquals,           METH_VARARGS },
#if SDL_VERSION_ATLEAST(2,0,22)
    { "HasIntersectionF",     PySDL_HasIntersectionF,     METH_VARARGS },
    { "IntersectFRect",       PySDL_IntersectFRect,       METH_VARARGS },
    { "UnionFRect",           PySDL_UnionFRect,           METH_VARARGS },
    { "PointInFRect",         PySDL_PointInFRect,         METH_VARARGS },
    { "FRectEmpty",           PySDL_FRectEmpty,           METH_O       },
    { "FRectEquals",          PySDL_FRectEquals,          METH_VARARGS },
#endif
    { NULL }
};
