#include "pysdl.h"

// The event queue: the `_event()` converter (SDL_Event -> (type, data)), the
// pump/poll/wait/peep/push functions, and an optional Python event filter.
// Registered in PyInit_SDL3 via PyModule_AddFunctions(module, pysdl_events_methods).

//=========================================================
// SDL_Event -> (type, data)
//=========================================================

static PyObject * _event(SDL_Event *event) {
    PyObject *data;

    switch(event->type) {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
            SDL_KeyboardEvent *e = &event->key;
            data = Py_BuildValue("(iiiiO)", e->down, e->scancode,
                e->key, e->mod, e->repeat ? Py_True : Py_False);
        }
        break;
    case SDL_EVENT_TEXT_INPUT:
        data = PyUnicode_FromString(event->text.text ? event->text.text : "");
        break;
    case SDL_EVENT_TEXT_EDITING:
        data = Py_BuildValue("(sii)", event->edit.text ? event->edit.text : "",
            event->edit.start, event->edit.length);
        break;
    case SDL_EVENT_MOUSE_MOTION: {
            SDL_MouseMotionEvent *e = &event->motion;
            data = Py_BuildValue("(Iffff)", e->state, e->x, e->y, e->xrel, e->yrel);
        }
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
            SDL_MouseButtonEvent *e = &event->button;
            data = Py_BuildValue("(iiiff)", e->which, e->button, e->down, e->x, e->y);
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL: {
            SDL_MouseWheelEvent *e = &event->wheel;
            data = Py_BuildValue("(iffi)", e->which, e->x, e->y, e->direction);
        }
        break;
    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        data = Py_BuildValue("(iii)", event->jaxis.which, event->jaxis.axis, event->jaxis.value);
        break;
    case SDL_EVENT_JOYSTICK_BALL_MOTION:
        data = Py_BuildValue("(iiii)", event->jball.which, event->jball.ball,
            event->jball.xrel, event->jball.yrel);
        break;
    case SDL_EVENT_JOYSTICK_HAT_MOTION:
        data = Py_BuildValue("(iii)", event->jhat.which, event->jhat.hat, event->jhat.value);
        break;
    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
    case SDL_EVENT_JOYSTICK_BUTTON_UP:
        data = Py_BuildValue("(iii)", event->jbutton.which, event->jbutton.button, event->jbutton.down);
        break;
    case SDL_EVENT_JOYSTICK_ADDED:
    case SDL_EVENT_JOYSTICK_REMOVED:
        data = Py_BuildValue("(i)", event->jdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        data = Py_BuildValue("(iii)", event->gaxis.which, event->gaxis.axis, event->gaxis.value);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        data = Py_BuildValue("(iii)", event->gbutton.which, event->gbutton.button, event->gbutton.down);
        break;
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED:
    case SDL_EVENT_GAMEPAD_REMAPPED:
        data = Py_BuildValue("(i)", event->gdevice.which);
        break;
    case SDL_EVENT_SENSOR_UPDATE: {
            const float *v = event->sensor.data;
            data = Py_BuildValue("(i(ffffff))", event->sensor.which,
                v[0], v[1], v[2], v[3], v[4], v[5]);
        }
        break;
    case SDL_EVENT_AUDIO_DEVICE_ADDED:
    case SDL_EVENT_AUDIO_DEVICE_REMOVED:
        data = Py_BuildValue("(Ii)", event->adevice.which, event->adevice.recording);
        break;
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_UP: {
            SDL_TouchFingerEvent *e = &event->tfinger;
            data = Py_BuildValue("(LLfffffI)", (long long)e->touchID, (long long)e->fingerID,
                e->x, e->y, e->dx, e->dy, e->pressure, e->windowID);
        }
        break;
    case SDL_EVENT_DROP_FILE:
    case SDL_EVENT_DROP_TEXT:
        // SDL3 owns drop.data; do not free it.
        data = Py_BuildValue("(sI)", event->drop.data ? event->drop.data : "", event->drop.windowID);
        break;
    case SDL_EVENT_DROP_BEGIN:
    case SDL_EVENT_DROP_COMPLETE:
        data = Py_BuildValue("(I)", event->drop.windowID);
        break;
    case SDL_EVENT_QUIT:
        data = Py_NewRef(Py_None);
        break;
    default:
        if(event->type >= SDL_EVENT_WINDOW_FIRST && event->type <= SDL_EVENT_WINDOW_LAST) {
            SDL_WindowEvent *e = &event->window;
            data = Py_BuildValue("(iiiI)", event->type, e->data1, e->data2, e->windowID);
        } else if(event->type >= SDL_EVENT_DISPLAY_FIRST && event->type <= SDL_EVENT_DISPLAY_LAST) {
            SDL_DisplayEvent *e = &event->display;
            data = Py_BuildValue("(iiiI)", event->type, e->data1, e->data2, e->displayID);
        } else if(event->type >= SDL_EVENT_USER) {
            data = Py_BuildValue("(iI)", event->user.code, event->user.windowID);
        } else {
            data = Py_NewRef(Py_None);
        }
    }

    if(NULL == data) {
        return NULL;
    }
    return Py_BuildValue("(IN)", event->type, data);
}

//=========================================================
// pump / poll / wait
//=========================================================

static PyObject * PySDL_PumpEvents(PyObject *self, PyObject *ign) {
    SDL_PumpEvents();
    Py_RETURN_NONE;
}

static PyObject * PySDL_PollEvent(PyObject *self, PyObject *ign) {
    SDL_Event event;
    if(!SDL_PollEvent(&event)) {
        Py_RETURN_NONE;
    }
    return _event(&event);
}

static PyObject * PySDL_WaitEvent(PyObject *self, PyObject *ign) {
    SDL_Event event;
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_WaitEvent(&event);
    Py_END_ALLOW_THREADS
    if(!ok) {
        Py_RETURN_NONE;
    }
    return _event(&event);
}

static PyObject * PySDL_WaitEventTimeout(PyObject *self, PyObject *arg) {
    long ms = PyLong_AsLong(arg);
    if(-1 == ms && PyErr_Occurred()) {
        return NULL;
    }
    SDL_Event event;
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_WaitEventTimeout(&event, (Sint32)ms);
    Py_END_ALLOW_THREADS
    if(!ok) {
        Py_RETURN_NONE;
    }
    return _event(&event);
}

//=========================================================
// queue manipulation
//=========================================================

static PyObject * PySDL_PushEvent(PyObject *self, PyObject *args, PyObject *kwds) {
    unsigned int type;
    int code = 0;
    unsigned int windowID = 0;

    static char *kwlist[] = {"type", "code", "windowID", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "I|iI", kwlist, &type, &code, &windowID)) {
        return NULL;
    }

    SDL_Event event;
    SDL_memset(&event, 0, sizeof(event));
    event.type = type;
    event.user.code = code;
    event.user.windowID = windowID;

    // SDL3 returns false when a filter drops the event (no error set) as well as
    // on real failure; surface the former as False, the latter as an exception.
    bool ok = SDL_PushEvent(&event);
    if(!ok) {
        const char *err = SDL_GetError();
        if(err && err[0]) {
            PyErr_SetString(pysdl_Error, err);
            return NULL;
        }
    }
    return PyBool_FromLong(ok);
}

static PyObject * PySDL_PeepEvents(PyObject *self, PyObject *args) {
    int numevents;
    int action = SDL_PEEKEVENT;
    unsigned int minType = SDL_EVENT_FIRST;
    unsigned int maxType = SDL_EVENT_LAST;

    if(!PyArg_ParseTuple(args, "i|iII", &numevents, &action, &minType, &maxType)) {
        return NULL;
    }
    if(SDL_ADDEVENT == action) {
        PyErr_SetString(PyExc_ValueError, "PeepEvents cannot ADD - use PushEvent");
        return NULL;
    }
    if(numevents <= 0) {
        return PyList_New(0);
    }

    SDL_Event *events = PyMem_New(SDL_Event, numevents);
    if(NULL == events) {
        return PyErr_NoMemory();
    }

    int got;
    Py_BEGIN_ALLOW_THREADS
        got = SDL_PeepEvents(events, numevents, (SDL_EventAction)action, minType, maxType);
    Py_END_ALLOW_THREADS
    if(0 > got) {
        PyMem_Free(events);
        PyErr_SetString(pysdl_Error, SDL_GetError());
        return NULL;
    }

    PyObject *list = PyList_New(got);
    if(NULL == list) {
        PyMem_Free(events);
        return NULL;
    }
    for(int idx = 0; idx < got; ++idx) {
        PyObject *item = _event(&events[idx]);
        if(NULL == item) {
            Py_DECREF(list);
            PyMem_Free(events);
            return NULL;
        }
        PyList_SET_ITEM(list, idx, item);
    }
    PyMem_Free(events);
    return list;
}

static PyObject * PySDL_FlushEvent(PyObject *self, PyObject *arg) {
    long type = PyLong_AsLong(arg);
    if(-1 == type && PyErr_Occurred()) {
        return NULL;
    }
    SDL_FlushEvent((Uint32)type);
    Py_RETURN_NONE;
}

static PyObject * PySDL_FlushEvents(PyObject *self, PyObject *args) {
    unsigned int minType = SDL_EVENT_FIRST;
    unsigned int maxType = SDL_EVENT_LAST;
    if(!PyArg_ParseTuple(args, "|II", &minType, &maxType)) {
        return NULL;
    }
    SDL_FlushEvents(minType, maxType);
    Py_RETURN_NONE;
}

static PyObject * PySDL_HasEvent(PyObject *self, PyObject *arg) {
    long type = PyLong_AsLong(arg);
    if(-1 == type && PyErr_Occurred()) {
        return NULL;
    }
    return PyBool_FromLong(SDL_HasEvent((Uint32)type));
}

static PyObject * PySDL_HasEvents(PyObject *self, PyObject *args) {
    unsigned int minType = SDL_EVENT_FIRST;
    unsigned int maxType = SDL_EVENT_LAST;
    if(!PyArg_ParseTuple(args, "|II", &minType, &maxType)) {
        return NULL;
    }
    return PyBool_FromLong(SDL_HasEvents(minType, maxType));
}

static PyObject * PySDL_RegisterEvents(PyObject *self, PyObject *arg) {
    long numevents = PyLong_AsLong(arg);
    if(-1 == numevents && PyErr_Occurred()) {
        return NULL;
    }
    Uint32 base = SDL_RegisterEvents((int)numevents);
    if((Uint32)-1 == base) {
        PyErr_SetString(pysdl_Error, "could not register events (queue full)");
        return NULL;
    }
    return PyLong_FromUnsignedLong(base);
}

// SDL3: SDL_EventState/SDL_QUERY -> SDL_SetEventEnabled / SDL_EventEnabled.
static PyObject * PySDL_EventState(PyObject *self, PyObject *args) {
    unsigned int type;
    int state;
    if(!PyArg_ParseTuple(args, "Ii", &type, &state)) {
        return NULL;
    }
    bool was = SDL_EventEnabled(type);
    SDL_SetEventEnabled(type, state ? true : false);
    return PyBool_FromLong(was);
}

static PyObject * PySDL_GetEventState(PyObject *self, PyObject *arg) {
    long type = PyLong_AsLong(arg);
    if(-1 == type && PyErr_Occurred()) {
        return NULL;
    }
    return PyBool_FromLong(SDL_EventEnabled((Uint32)type));
}

static PyObject * PySDL_QuitRequested(PyObject *self, PyObject *ign) {
    SDL_PumpEvents();
    int pending = SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT);
    return PyBool_FromLong(pending > 0);
}

//=========================================================
// event filter (optional / advanced)
//=========================================================

static PyObject *_py_event_filter = NULL;

// Run `callable` on the converted event; returns true to keep it, false to drop.
static bool _run_filter(PyObject *callable, SDL_Event *event) {
    PyGILState_STATE gil;
    if(!PySDL_ThreadEnter(&gil)) {
        return true;  // interpreter gone: keep the event, do nothing
    }

    bool keep = true;
    PyObject *converted = _event(event);
    if(NULL == converted) {
        PyErr_Print();
    } else {
        PyObject *result = PyObject_CallFunctionObjArgs(callable, converted, NULL);
        Py_DECREF(converted);
        if(NULL == result) {
            PyErr_Print();
        } else {
            int truth = PyObject_IsTrue(result);
            Py_DECREF(result);
            if(-1 == truth) {
                PyErr_Print();
            } else {
                keep = truth != 0;
            }
        }
    }

    PySDL_ThreadLeave(gil);
    return keep;
}

static bool SDLCALL _c_event_filter(void *userdata, SDL_Event *event) {
    (void)userdata;
    return NULL == _py_event_filter ? true : _run_filter(_py_event_filter, event);
}

static bool SDLCALL _c_filter_once(void *userdata, SDL_Event *event) {
    return _run_filter((PyObject *)userdata, event);
}

static PyObject * PySDL_SetEventFilter(PyObject *self, PyObject *arg) {
    if(arg == Py_None) {
        SDL_SetEventFilter(NULL, NULL);
        Py_CLEAR(_py_event_filter);
        Py_RETURN_NONE;
    }
    if(!PyCallable_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "expected a callable or None");
        return NULL;
    }
    Py_INCREF(arg);
    Py_XSETREF(_py_event_filter, arg);
    SDL_SetEventFilter(_c_event_filter, NULL);
    Py_RETURN_NONE;
}

static PyObject * PySDL_GetEventFilter(PyObject *self, PyObject *ign) {
    if(NULL == _py_event_filter) {
        Py_RETURN_NONE;
    }
    return Py_NewRef(_py_event_filter);
}

static PyObject * PySDL_FilterEvents(PyObject *self, PyObject *arg) {
    if(!PyCallable_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "expected a callable");
        return NULL;
    }
    SDL_FilterEvents(_c_filter_once, arg);  // synchronous; arg stays alive
    Py_RETURN_NONE;
}

PyMethodDef pysdl_events_methods[] = {
    { "PumpEvents",        PySDL_PumpEvents,        METH_NOARGS  },
    { "PollEvent",         PySDL_PollEvent,         METH_NOARGS  },
    { "WaitEvent",         PySDL_WaitEvent,         METH_NOARGS  },
    { "WaitEventTimeout",  PySDL_WaitEventTimeout,  METH_O       },
    { "PushEvent",         (PyCFunction)PySDL_PushEvent, METH_VARARGS | METH_KEYWORDS },
    { "PeepEvents",        PySDL_PeepEvents,        METH_VARARGS },
    { "FlushEvent",        PySDL_FlushEvent,        METH_O       },
    { "FlushEvents",       PySDL_FlushEvents,       METH_VARARGS },
    { "HasEvent",          PySDL_HasEvent,          METH_O       },
    { "HasEvents",         PySDL_HasEvents,         METH_VARARGS },
    { "RegisterEvents",    PySDL_RegisterEvents,    METH_O       },
    { "EventState",        PySDL_EventState,        METH_VARARGS },
    { "GetEventState",     PySDL_GetEventState,     METH_O       },
    { "SetEventEnabled",   PySDL_EventState,        METH_VARARGS },
    { "EventEnabled",      PySDL_GetEventState,     METH_O       },
    { "QuitRequested",     PySDL_QuitRequested,     METH_NOARGS  },
    { "SetEventFilter",    PySDL_SetEventFilter,    METH_O       },
    { "GetEventFilter",    PySDL_GetEventFilter,    METH_NOARGS  },
    { "FilterEvents",      PySDL_FilterEvents,      METH_O       },
    { NULL }
};
