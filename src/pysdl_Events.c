#include "pysdl.h"

// The event queue: the `_event()` converter (SDL_Event -> (type, data)), the
// pump/poll/wait/peep/push functions, an optional Python event filter, and
// event watchers.
//
// GIL rule: SDL runs the filter and the watchers under its event-watcher lock,
// on whichever thread pushed the event (SDL_SetEventFilter also runs the new
// filter over the queue holding the queue lock), and our trampolines then take
// the GIL - which the Python callback may hand to another thread mid-call. So
// every call here that can take either lock (anything that pushes, pumps,
// polls, peeks or flushes, or (un)registers a filter / watch) drops the GIL
// first; holding it there deadlocks against a callback on another thread.
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
    case SDL_EVENT_JOYSTICK_UPDATE_COMPLETE:
        data = Py_BuildValue("(i)", event->jdevice.which);
        break;
    case SDL_EVENT_JOYSTICK_BATTERY_UPDATED:
        // SDL3: a POWERSTATE_* plus percent (-1 if unknown) replace SDL2's level.
        data = Py_BuildValue("(iii)", event->jbattery.which, event->jbattery.state,
            event->jbattery.percent);
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
    case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE:
    case SDL_EVENT_GAMEPAD_STEAM_HANDLE_UPDATED:
        data = Py_BuildValue("(i)", event->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
    case SDL_EVENT_GAMEPAD_TOUCHPAD_UP: {
            SDL_GamepadTouchpadEvent *e = &event->gtouchpad;
            data = Py_BuildValue("(iiifff)", e->which, e->touchpad, e->finger, e->x, e->y, e->pressure);
        }
        break;
    case SDL_EVENT_GAMEPAD_SENSOR_UPDATE: {
            // SDL3 dropped GetSensorDataWithTimestamp; the reading's own
            // timestamp (ns) now arrives only here.
            SDL_GamepadSensorEvent *e = &event->gsensor;
            data = Py_BuildValue("(ii(fff)K)", e->which, e->sensor, e->data[0], e->data[1],
                e->data[2], (unsigned long long)e->sensor_timestamp);
        }
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
    Py_BEGIN_ALLOW_THREADS
        SDL_PumpEvents();
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyObject * PySDL_PollEvent(PyObject *self, PyObject *ign) {
    SDL_Event event;
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_PollEvent(&event);
    Py_END_ALLOW_THREADS
    if(!ok) {
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
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_PushEvent(&event);
    Py_END_ALLOW_THREADS
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
    Py_BEGIN_ALLOW_THREADS
        SDL_FlushEvent((Uint32)type);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyObject * PySDL_FlushEvents(PyObject *self, PyObject *args) {
    unsigned int minType = SDL_EVENT_FIRST;
    unsigned int maxType = SDL_EVENT_LAST;
    if(!PyArg_ParseTuple(args, "|II", &minType, &maxType)) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
        SDL_FlushEvents(minType, maxType);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

static PyObject * PySDL_HasEvent(PyObject *self, PyObject *arg) {
    long type = PyLong_AsLong(arg);
    if(-1 == type && PyErr_Occurred()) {
        return NULL;
    }
    bool has;
    Py_BEGIN_ALLOW_THREADS
        has = SDL_HasEvent((Uint32)type);
    Py_END_ALLOW_THREADS
    return PyBool_FromLong(has);
}

static PyObject * PySDL_HasEvents(PyObject *self, PyObject *args) {
    unsigned int minType = SDL_EVENT_FIRST;
    unsigned int maxType = SDL_EVENT_LAST;
    if(!PyArg_ParseTuple(args, "|II", &minType, &maxType)) {
        return NULL;
    }
    bool has;
    Py_BEGIN_ALLOW_THREADS
        has = SDL_HasEvents(minType, maxType);
    Py_END_ALLOW_THREADS
    return PyBool_FromLong(has);
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
    Py_BEGIN_ALLOW_THREADS  // disabling flushes that type from the queue
        SDL_SetEventEnabled(type, state ? true : false);
    Py_END_ALLOW_THREADS
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
    int pending;
    Py_BEGIN_ALLOW_THREADS
        SDL_PumpEvents();
        pending = SDL_PeepEvents(NULL, 0, SDL_PEEKEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT);
    Py_END_ALLOW_THREADS
    return PyBool_FromLong(pending > 0);
}

//=========================================================
// event filter and watchers
//=========================================================

// Call `callable(event)` and return whether to keep the event: its truth, or
// true on an error (printed). The GIL is held and no exception is pending.
static bool _call_filter(PyObject *callable, SDL_Event *event) {
    // Hold our own reference: the callable may unregister itself mid-call.
    Py_INCREF(callable);
    bool keep = true;
    PyObject *converted = _event(event);
    PyObject *result = converted ? PyObject_CallFunctionObjArgs(callable, converted, NULL) : NULL;
    Py_XDECREF(converted);
    Py_DECREF(callable);
    if(NULL != result) {
        int truth = PyObject_IsTrue(result);
        Py_DECREF(result);
        if(-1 != truth) {
            keep = truth != 0;
        }
    }
    if(PyErr_Occurred()) {
        PyErr_Print();
    }
    return keep;
}

// Trampoline entry / exit. SDL can run the filter and watchers from inside
// another SDL call - even one a dealloc makes while an exception unwinds (a
// destroyed window pushes an event) - so a pending exception is set aside.
typedef struct {
    PyGILState_STATE gil;
    PyObject *etype, *evalue, *etb;
} _entered;

static int _enter(_entered *state) {
    if(!PySDL_ThreadEnter(&state->gil)) {
        return 0;  // interpreter gone: touch nothing
    }
    PyErr_Fetch(&state->etype, &state->evalue, &state->etb);
    return 1;
}

static void _leave(_entered *state) {
    PyErr_Restore(state->etype, state->evalue, state->etb);
    PySDL_ThreadLeave(state->gil);
}

// The filter is never handed to SDL: SDL's userdata is NULL and the
// trampoline reads _py_event_filter under the GIL, so replacing the filter
// can't free one a call on another thread is about to use. (It used to be
// read before the GIL was taken, while SetEventFilter released the old filter
// before telling SDL.)
static PyObject *_py_event_filter = NULL;

static bool SDLCALL _c_event_filter(void *userdata, SDL_Event *event) {
    (void)userdata;
    _entered state;
    if(!_enter(&state)) {
        return true;  // keep the event
    }
    bool keep = (NULL == _py_event_filter) ? true : _call_filter(_py_event_filter, event);
    _leave(&state);
    return keep;
}

// FilterEvents: synchronous, so the callable (the caller's argument) outlives it.
static bool SDLCALL _c_filter_once(void *userdata, SDL_Event *event) {
    _entered state;
    if(!_enter(&state)) {
        return true;
    }
    bool keep = _call_filter((PyObject *)userdata, event);
    _leave(&state);
    return keep;
}

static PyObject * PySDL_SetEventFilter(PyObject *self, PyObject *arg) {
    if(arg != Py_None && !PyCallable_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "expected a callable or None");
        return NULL;
    }
    PyObject *old = _py_event_filter;
    _py_event_filter = (arg == Py_None) ? NULL : Py_NewRef(arg);
    bool set = NULL != _py_event_filter;
    // A new filter also runs over the events already queued, right here.
    Py_BEGIN_ALLOW_THREADS
        SDL_SetEventFilter(set ? _c_event_filter : NULL, NULL);
    Py_END_ALLOW_THREADS
    Py_XDECREF(old);
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
    Py_BEGIN_ALLOW_THREADS
        SDL_FilterEvents(_c_filter_once, arg);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

// Event watchers use the Timer's token design (pysdl_Timer.c): SDL's userdata
// is an integer token and the trampoline looks the callable up - under the
// GIL - in _py_event_watches (token -> callable, in registration order).
// RemoveEventWatch deletes the entry first, so a call SDL already has under
// way finds nothing, and SDL never holds a pointer to a Python object.
static PyObject *_py_event_watches = NULL;
static unsigned long long _watch_next_token;

// Watchers see each event as it is pushed, after the filter (so not the ones
// it drops), before it is queued; their return value is ignored.
static bool SDLCALL _c_event_watch(void *token, SDL_Event *event) {
    _entered state;
    if(!_enter(&state)) {
        return true;
    }
    PyObject *key = PyLong_FromUnsignedLongLong((unsigned long long)(uintptr_t)token);
    PyObject *callable = (key && _py_event_watches) ? PyDict_GetItemWithError(_py_event_watches, key) : NULL;
    Py_XDECREF(key);
    if(NULL != callable) {
        _call_filter(callable, event);  // takes its own reference first
    } else if(PyErr_Occurred()) {
        PyErr_Print();
    }
    _leave(&state);
    return true;
}

// AddEventWatch(callback): callback(event) runs for every pushed event the
// filter keeps, on whichever thread pushed it. Adding a callback twice
// registers it twice.
static PyObject * PySDL_AddEventWatch(PyObject *self, PyObject *arg) {
    if(!PyCallable_Check(arg)) {
        PyErr_SetString(PyExc_TypeError, "expected a callable");
        return NULL;
    }
    if(NULL == _py_event_watches && NULL == (_py_event_watches = PyDict_New())) {
        return NULL;
    }
    unsigned long long token = ++_watch_next_token;
    PyObject *key = PyLong_FromUnsignedLongLong(token);
    if(NULL == key || 0 > PyDict_SetItem(_py_event_watches, key, arg)) {
        Py_XDECREF(key);
        return NULL;
    }
    bool ok;
    Py_BEGIN_ALLOW_THREADS
        ok = SDL_AddEventWatch(_c_event_watch, (void *)(uintptr_t)token);
    Py_END_ALLOW_THREADS
    if(!ok) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        PyObject *etype, *evalue, *etb;
        PyErr_Fetch(&etype, &evalue, &etb);
        if(0 > PyDict_DelItem(_py_event_watches, key)) {
            PyErr_Clear();
        }
        PyErr_Restore(etype, evalue, etb);
        Py_DECREF(key);
        return NULL;
    }
    Py_DECREF(key);
    Py_RETURN_NONE;
}

// RemoveEventWatch(callback) / DelEventWatch: drop the latest registration of
// callback (matched by identity); ValueError if there is none.
static PyObject * PySDL_RemoveEventWatch(PyObject *self, PyObject *arg) {
    PyObject *key = NULL;
    PyObject *k, *v;
    Py_ssize_t pos = 0;
    while(_py_event_watches && PyDict_Next(_py_event_watches, &pos, &k, &v)) {
        if(v == arg) {
            key = k;  // keep going: the last match is the latest registration
        }
    }
    if(NULL == key) {
        PyErr_SetString(PyExc_ValueError, "callback is not a registered event watch");
        return NULL;
    }
    unsigned long long token = PyLong_AsUnsignedLongLong(key);
    Py_INCREF(key);
    int rc = PyDict_DelItem(_py_event_watches, key);
    Py_DECREF(key);
    if(0 > rc) {
        return NULL;
    }
    Py_BEGIN_ALLOW_THREADS
        SDL_RemoveEventWatch(_c_event_watch, (void *)(uintptr_t)token);
    Py_END_ALLOW_THREADS
    Py_RETURN_NONE;
}

// The event subsystem stopped (SDL_QuitEvents): SDL dropped the filter and
// every watcher, so drop ours too (pysdl.c, after Quit / QuitSubSystem).
void PySDL_EventWatchesGone(void) {
    Py_CLEAR(_py_event_filter);
    if(NULL != _py_event_watches) {
        PyDict_Clear(_py_event_watches);
    }
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
    { "AddEventWatch",     PySDL_AddEventWatch,     METH_O       },
    { "RemoveEventWatch",  PySDL_RemoveEventWatch,  METH_O       },
    { "DelEventWatch",     PySDL_RemoveEventWatch,  METH_O       },
    { NULL }
};
