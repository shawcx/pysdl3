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
    .tp_doc       = "SDL3.Sensor(instance_id)",
    .tp_methods   = PySDL_Sensor_methods,
    .tp_init      = (initproc)PySDL_Sensor_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_Sensor_Type_init(PySDL_Sensor *self, PyObject *args, PyObject *kwds) {
    long long id = -1;
    if(!PyArg_ParseTuple(args, "|L", &id)) {
        return -1;
    }
    self->sensor = NULL;
    if(id > 0) {
        self->sensor = SDL_OpenSensor((SDL_SensorID)id);
        if(NULL == self->sensor) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
            return -1;
        }
    }
    return 0;
}

static void PySDL_Sensor_Type_dealloc(PySDL_Sensor *self) {
    if(NULL != self->sensor) {
        SDL_CloseSensor(self->sensor);
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
    const char *name = SDL_GetSensorName(s);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_Sensor_GetType(PySDL_Sensor *self, PyObject *ign) {
    SDL_Sensor *s = _s(self);
    return s ? PyLong_FromLong(SDL_GetSensorType(s)) : NULL;
}

static PyObject * PySDL_Sensor_GetNonPortableType(PySDL_Sensor *self, PyObject *ign) {
    SDL_Sensor *s = _s(self);
    return s ? PyLong_FromLong(SDL_GetSensorNonPortableType(s)) : NULL;
}

static PyObject * PySDL_Sensor_GetInstanceID(PySDL_Sensor *self, PyObject *ign) {
    SDL_Sensor *s = _s(self);
    return s ? PyLong_FromUnsignedLong(SDL_GetSensorID(s)) : NULL;
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
    if(!SDL_GetSensorData(s, values, count)) {
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
        SDL_CloseSensor(self->sensor);
        self->sensor = NULL;
    }
    Py_RETURN_NONE;
}

//=========================================================
// module functions
//=========================================================

static PyObject * PySDL_NumSensors(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_SensorID *ids = SDL_GetSensors(&count);
    SDL_free(ids);
    return PyLong_FromLong(count);
}

static PyObject * PySDL_GetSensors(PyObject *self, PyObject *ign) {
    int count = 0;
    SDL_SensorID *ids = SDL_GetSensors(&count);
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

static PyObject * PySDL_SensorGetDeviceName(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    const char *name = SDL_GetSensorNameForID((SDL_SensorID)id);
    return PyUnicode_FromString(name ? name : "");
}

static PyObject * PySDL_SensorGetDeviceType(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetSensorTypeForID((SDL_SensorID)id));
}

static PyObject * PySDL_SensorGetDeviceNonPortableType(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    return PyLong_FromLong(SDL_GetSensorNonPortableTypeForID((SDL_SensorID)id));  // -1 if unknown
}

// SensorFromInstanceID(id) -> a new, owned Sensor for an already-open one
// (re-opening by ID bumps SDL's refcount, as the joystick lookups do), or None.
static PyObject * PySDL_SensorFromInstanceID(PyObject *self, PyObject *arg) {
    unsigned long id = PyLong_AsUnsignedLong(arg);
    if((unsigned long)-1 == id && PyErr_Occurred()) {
        return NULL;
    }
    if(NULL == SDL_GetSensorFromID((SDL_SensorID)id)) {
        Py_RETURN_NONE;
    }
    return PyObject_CallFunction((PyObject *)&PySDL_Sensor_Type, "k", id);
}

static PyObject * PySDL_SensorGetDeviceInstanceID(PyObject *self, PyObject *arg) {
    // In SDL3 the "device" is already identified by its SDL_SensorID.
    return Py_NewRef(arg);
}

static PyObject * PySDL_SensorUpdate(PyObject *self, PyObject *ign) {
    SDL_UpdateSensors();
    Py_RETURN_NONE;
}

PyMethodDef pysdl_sensor_methods[] = {
    { "NumSensors",                 PySDL_NumSensors,                 METH_NOARGS },
    { "GetSensors",                 PySDL_GetSensors,                 METH_NOARGS },
    { "SensorGetDeviceName",        PySDL_SensorGetDeviceName,        METH_O      },
    { "GetSensorNameForID",         PySDL_SensorGetDeviceName,        METH_O      },
    { "SensorGetDeviceType",        PySDL_SensorGetDeviceType,        METH_O      },
    { "GetSensorTypeForID",         PySDL_SensorGetDeviceType,        METH_O      },
    { "SensorGetDeviceNonPortableType", PySDL_SensorGetDeviceNonPortableType, METH_O },
    { "GetSensorNonPortableTypeForID",  PySDL_SensorGetDeviceNonPortableType, METH_O },
    { "SensorFromInstanceID",       PySDL_SensorFromInstanceID,       METH_O      },
    { "GetSensorFromID",            PySDL_SensorFromInstanceID,       METH_O      },
    { "SensorGetDeviceInstanceID",  PySDL_SensorGetDeviceInstanceID,  METH_O      },
    { "SensorUpdate",               PySDL_SensorUpdate,               METH_NOARGS },
    { NULL }
};
