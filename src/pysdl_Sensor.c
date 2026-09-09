#include "pysdl.h"

static int        PySDL_Sensor_Type_init    (PySDL_Sensor*, PyObject*, PyObject*);
static void       PySDL_Sensor_Type_dealloc (PySDL_Sensor*);

static PyObject * PySDL_Sensor_GetName           (PySDL_Sensor*, PyObject*);
static PyObject * PySDL_Sensor_GetType           (PySDL_Sensor*, PyObject*);
static PyObject * PySDL_Sensor_GetNonPortableType(PySDL_Sensor*, PyObject*);
static PyObject * PySDL_Sensor_GetInstanceID     (PySDL_Sensor*, PyObject*);
static PyObject * PySDL_Sensor_GetData           (PySDL_Sensor*, PyObject*, PyObject*);
static PyObject * PySDL_Sensor_Close             (PySDL_Sensor*, PyObject*);

static PyMethodDef PySDL_Sensor_methods[] = {
    { "GetName",            (PyCFunction)PySDL_Sensor_GetName,            METH_NOARGS },
    { "GetType",            (PyCFunction)PySDL_Sensor_GetType,            METH_NOARGS },
    { "GetNonPortableType", (PyCFunction)PySDL_Sensor_GetNonPortableType, METH_NOARGS },
    { "GetInstanceID",      (PyCFunction)PySDL_Sensor_GetInstanceID,      METH_NOARGS },
    { "GetData",            (PyCFunction)PySDL_Sensor_GetData,            METH_VARARGS | METH_KEYWORDS },
    { "Close",              (PyCFunction)PySDL_Sensor_Close,              METH_NOARGS },
    { NULL }
};

PyTypeObject PySDL_Sensor_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Sensor",
    .tp_basicsize = sizeof(PySDL_Sensor),
    .tp_dealloc   = (destructor)PySDL_Sensor_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Sensor(device_index)",
    .tp_methods   = PySDL_Sensor_methods,
    .tp_init      = (initproc)PySDL_Sensor_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_Sensor_Type_init(PySDL_Sensor *self, PyObject *args, PyObject *kwds) {
    int index = -1;
    if(!PyArg_ParseTuple(args, "|i", &index)) {
        return -1;
    }
    self->sensor = NULL;
    if(index >= 0) {
        self->sensor = SDL_SensorOpen(index);
        if(NULL == self->sensor) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_Sensor_Type_dealloc(PySDL_Sensor *self) {
    if(NULL != self->sensor) {
        SDL_SensorClose(self->sensor);
        self->sensor = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static SDL_Sensor * _s(PySDL_Sensor *self) {
    if(NULL == self->sensor) {
        PyErr_SetString(pysdl_Error, "Sensor is not open");
        return NULL;
    }
    return self->sensor;
}

static PyObject * PySDL_Sensor_GetName(PySDL_Sensor *self, PyObject *ign) {
    SDL_Sensor *s = _s(self);
    if(NULL == s) return NULL;
    const char *name = SDL_SensorGetName(s);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_Sensor_GetType(PySDL_Sensor *self, PyObject *ign) {
    SDL_Sensor *s = _s(self);
    return s ? PyLong_FromLong(SDL_SensorGetType(s)) : NULL;
}

static PyObject * PySDL_Sensor_GetNonPortableType(PySDL_Sensor *self, PyObject *ign) {
    SDL_Sensor *s = _s(self);
    return s ? PyLong_FromLong(SDL_SensorGetNonPortableType(s)) : NULL;
}

static PyObject * PySDL_Sensor_GetInstanceID(PySDL_Sensor *self, PyObject *ign) {
    SDL_Sensor *s = _s(self);
    return s ? PyLong_FromLong(SDL_SensorGetInstanceID(s)) : NULL;
}

static PyObject * PySDL_Sensor_GetData(PySDL_Sensor *self, PyObject *args, PyObject *kwds) {
    int count = 6;
    SDL_Sensor *s = _s(self);
    static char *kwlist[] = {"count", NULL};
    if(NULL == s || !PyArg_ParseTupleAndKeywords(args, kwds, "|i", kwlist, &count)) {
        return NULL;
    }
    if(count < 1 || count > 16) {
        PyErr_SetString(PyExc_ValueError, "count must be 1..16");
        return NULL;
    }

    float values[16];
    if(0 > SDL_SensorGetData(s, values, count)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    PyObject *tuple = PyTuple_New(count);
    if(NULL == tuple) {
        return NULL;
    }
    for(int idx = 0; idx < count; ++idx) {
        PyObject *f = PyFloat_FromDouble(values[idx]);
        if(NULL == f) {
            Py_DECREF(tuple);
            return NULL;
        }
        PyTuple_SET_ITEM(tuple, idx, f);
    }
    return tuple;
}

static PyObject * PySDL_Sensor_Close(PySDL_Sensor *self, PyObject *ign) {
    if(NULL != self->sensor) {
        SDL_SensorClose(self->sensor);
        self->sensor = NULL;
    }
    Py_RETURN_NONE;
}

//=========================================================
// module functions
//=========================================================

static PyObject * PySDL_NumSensors(PyObject *self, PyObject *ign) {
    return PyLong_FromLong(SDL_NumSensors());
}

static PyObject * PySDL_SensorGetDeviceName(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_SensorGetDeviceName((int)index);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_SensorGetDeviceType(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLong(SDL_SensorGetDeviceType((int)index));
}

static PyObject * PySDL_SensorGetDeviceInstanceID(PyObject *self, PyObject *arg) {
    long index = PyLong_AsLong(arg);
    if(-1 == index && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLong(SDL_SensorGetDeviceInstanceID((int)index));
}

static PyObject * PySDL_SensorUpdate(PyObject *self, PyObject *ign) {
    SDL_SensorUpdate();
    Py_RETURN_NONE;
}

PyMethodDef pysdl_sensor_methods[] = {
    { "NumSensors",                 PySDL_NumSensors,                 METH_NOARGS },
    { "SensorGetDeviceName",        PySDL_SensorGetDeviceName,        METH_O      },
    { "SensorGetDeviceType",        PySDL_SensorGetDeviceType,        METH_O      },
    { "SensorGetDeviceInstanceID",  PySDL_SensorGetDeviceInstanceID,  METH_O      },
    { "SensorUpdate",               PySDL_SensorUpdate,               METH_NOARGS },
    { NULL }
};
