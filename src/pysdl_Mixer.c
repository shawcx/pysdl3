#include "pysdl.h"

// SDL3_mixer (optional: compiled only when setup.py finds it, PYSDL_HAVE_MIXER).
//
// SDL3_mixer is not SDL2_mixer: there are no channels or music; a MIX_Mixer
// mixes MIX_Tracks, each fed by a MIX_Audio, an SDL_AudioStream or an
// SDL_IOStream, optionally gathered into MIX_Groups. Python types:
//   SDL3.Mixer        MIX_Mixer (on an audio device, or offline + Generate)
//   SDL3.MixerAudio   MIX_Audio (SDL3.Audio is already the device wrapper)
//   SDL3.MixerTrack   MIX_Track
//   SDL3.MixerGroup   MIX_Group
//   SDL3.MixerDecoder MIX_AudioDecoder
// Module functions keep the MIX_ prefix (like IMG_).
//
// Locking. The mixing thread (the device's audio thread, or the caller of
// Mixer.Generate) holds the mixer's mutex - MIX_LockMixer's - while it mixes
// and while it runs our callbacks, which then need the GIL. So every method of
// a mixer-bound object takes that mutex with the GIL released (_hold) and then
// runs with both, the same order as the mixing thread; the mutex is recursive,
// so callbacks may call back in. `busy` counts those holders: Destroy nulls
// the wrappers and then waits for it to drain before freeing the mixer.
//
// Callbacks. SDL gets an integer token, never a Python object: each Mixer /
// Track / Group wrapper has a token keying its callables in _callbacks, looked
// up under the GIL and dropped (under the mutex) before the object goes away.
// PCM callbacks exchange bytes copies, never a pointer into SDL's buffer.

#ifdef PYSDL_HAVE_MIXER

#include <SDL3_mixer/SDL_mixer.h>

typedef struct {
    PyObject_HEAD
    MIX_Mixer *mixer;
    int device;                // from MIX_CreateMixerDevice (destroy on the main thread)
    int busy;                  // holders of the mutex through _hold (and Lock())
    int locks;                 // Lock() depth, by `lock_thread`
    unsigned long lock_thread;
    MIX_Mixer *locked_mixer;   // what Lock() locked (->mixer may be nulled meanwhile)
    unsigned long long token;  // _callbacks key: [post_mix]
} PySDL_Mixer;

typedef struct {
    PyObject_HEAD
    MIX_Audio *audio;
} PySDL_MixerAudio;

typedef struct {
    PyObject_HEAD
    MIX_Track *track;
    PySDL_Mixer *owner;        // strong: the mixer outlives its track wrappers
    PyObject *input;           // the MixerAudio / AudioStream feeding it, or NULL
    unsigned long long token;  // _callbacks key: [stopped, raw, cooked]
} PySDL_MixerTrack;

typedef struct {
    PyObject_HEAD
    MIX_Group *group;
    PySDL_Mixer *owner;
    unsigned long long token;  // _callbacks key: [post_mix]
} PySDL_MixerGroup;

typedef struct {
    PyObject_HEAD
    MIX_AudioDecoder *decoder;
    int busy;                  // a Decode() in progress (GIL released)
} PySDL_MixerDecoder;

extern PyTypeObject PySDL_Mixer_Type;
extern PyTypeObject PySDL_MixerAudio_Type;
extern PyTypeObject PySDL_MixerTrack_Type;
extern PyTypeObject PySDL_MixerGroup_Type;
extern PyTypeObject PySDL_MixerDecoder_Type;

enum { SLOT_STOPPED, SLOT_RAW, SLOT_COOKED, SLOT_COUNT };  // track slots; mixer / group use slot 0

// Live wrappers, for teardown (MIX_Quit, Mixer.Destroy) and pointer lookups.
static PySDL_Registry _mixers, _audios, _tracks, _groups, _decoders;

static PyObject *_callbacks;             // token -> list of callables-or-None
static unsigned long long _next_token;
static int _inits;                       // successful MIX_Init calls not yet matched by MIX_Quit

// > 0 while this thread runs one of our mixer callbacks; _tls_pcm while it runs
// a track's raw / cooked one (SDL forbids touching the track there).
static _Thread_local int _tls_callback;
static _Thread_local int _tls_pcm;

static PyObject * _raise(void) {
    PyErr_SetString(pysdl_Error, SDL_GetError());
    return NULL;
}

static PyObject * _gone(const char *what) {
    PyErr_Format(pysdl_Error, "%s is not initialized (destroyed, or released by MIX_Quit)", what);
    return NULL;
}

//=========================================================
// shared helpers
//=========================================================

// (freq, format, channels), the order the rest of the binding uses.
static int _spec_from_py(PyObject *obj, SDL_AudioSpec *spec) {
    int freq, format, channels;
    if(!PyArg_ParseTuple(obj, "iii;an audio spec is (freq, format, channels)", &freq, &format, &channels)) {
        return 0;
    }
    spec->freq = freq;
    spec->format = (SDL_AudioFormat)format;
    spec->channels = channels;
    return 1;
}

static PyObject * _spec_to_py(const SDL_AudioSpec *spec) {
    return Py_BuildValue("(iii)", spec->freq, (int)spec->format, spec->channels);
}

// An SDL_IOStream that owns everything it reads: a path is opened as a file, a
// bytes-like is copied into a growable memory stream (the mixer may read it
// long after this call, so nothing of Python's is pinned). Pass closeio=true.
static SDL_IOStream * _owned_io(PyObject *src) {
    PyObject *path;
    int is_path = PySDL_PathFromObject(src, &path);
    if(0 > is_path) {
        return NULL;
    }
    if(is_path) {
        Py_buffer none;
        SDL_IOStream *io = PySDL_IOFromObject(path, &none);  // opens the file
        Py_DECREF(path);
        return io;
    }
    Py_buffer view;
    if(0 > PyObject_GetBuffer(src, &view, PyBUF_SIMPLE)) {
        if(PyErr_ExceptionMatches(PyExc_TypeError)) {
            PyErr_Format(PyExc_TypeError, "expected a path or the file's bytes, not %.200s",
                Py_TYPE(src)->tp_name);
        }
        return NULL;
    }
    SDL_IOStream *io = SDL_IOFromDynamicMem();
    if(NULL == io
        || (0 < view.len && (size_t)view.len != SDL_WriteIO(io, view.buf, (size_t)view.len))
        || 0 > SDL_SeekIO(io, 0, SDL_IO_SEEK_SET)) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
        if(NULL != io) {
            SDL_CloseIO(io);
        }
        io = NULL;
    }
    PyBuffer_Release(&view);
    return io;
}

// PyLong_AsInt is Python 3.13+.
static int _as_int(PyObject *obj, int *out) {
    long value = PyLong_AsLong(obj);
    if(-1 == value && PyErr_Occurred()) {
        return 0;
    }
    if(value < INT_MIN || value > INT_MAX) {
        PyErr_SetString(PyExc_OverflowError, "value does not fit in a C int");
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int _need_init(void) {
    if(0 == _inits) {
        PyErr_SetString(pysdl_Error, "SDL_mixer is not initialized: call MIX_Init() first");
        return 0;
    }
    return 1;
}

static int _not_in_callback(const char *what) {
    if(_tls_callback) {
        PyErr_Format(pysdl_Error, "%s cannot be called from a mixer callback", what);
        return 0;
    }
    return 1;
}

//=========================================================
// callback table
//=========================================================

static PyObject * _token_key(unsigned long long token) {
    return PyLong_FromUnsignedLongLong(token);
}

// Store `fn` (None clears) in `slot` of `token`'s entry. 0 with an exception on failure.
static int _slot_set(unsigned long long token, int slot, int nslots, PyObject *fn) {
    if(NULL == _callbacks && NULL == (_callbacks = PyDict_New())) {
        return 0;
    }
    PyObject *key = _token_key(token);
    if(NULL == key) {
        return 0;
    }
    PyObject *entry = PyDict_GetItemWithError(_callbacks, key);  // borrowed
    if(NULL == entry) {
        if(PyErr_Occurred() || NULL == (entry = PyList_New(nslots))) {
            Py_DECREF(key);
            return 0;
        }
        for(int idx = 0; idx < nslots; ++idx) {
            PyList_SET_ITEM(entry, idx, Py_NewRef(Py_None));
        }
        int rc = PyDict_SetItem(_callbacks, key, entry);
        Py_DECREF(entry);  // the dict holds it now
        if(0 > rc) {
            Py_DECREF(key);
            return 0;
        }
    }
    Py_DECREF(key);
    return 0 == PyList_SetItem(entry, slot, Py_NewRef(fn));
}

// The callable in `slot` of `token`'s entry (new ref), or NULL (none / gone).
static PyObject * _slot_get(unsigned long long token, int slot) {
    if(NULL == _callbacks) {
        return NULL;
    }
    PyObject *key = _token_key(token);
    PyObject *entry = key ? PyDict_GetItemWithError(_callbacks, key) : NULL;
    Py_XDECREF(key);
    if(NULL == entry) {
        PyErr_Clear();
        return NULL;
    }
    PyObject *fn = PyList_GET_ITEM(entry, slot);
    return (Py_None == fn) ? NULL : Py_NewRef(fn);
}

static void _slots_drop(unsigned long long token) {
    if(NULL == _callbacks || 0 == token) {
        return;
    }
    PyObject *et, *ev, *etb;
    PyErr_Fetch(&et, &ev, &etb);
    PyObject *key = _token_key(token);
    if(NULL == key || 0 > PyDict_DelItem(_callbacks, key)) {
        PyErr_Clear();  // nothing registered
    }
    Py_XDECREF(key);
    PyErr_Restore(et, ev, etb);
}

//=========================================================
// holding a mixer
//=========================================================

typedef struct {
    PySDL_Mixer *owner;
    MIX_Mixer   *mixer;
} _Held;

// Take `owner`'s mutex with the GIL released, then hold both; `busy` keeps the
// mixer alive meanwhile. 0 with an exception if it is gone. Re-check any other
// pointer afterwards: another thread may have destroyed it while we waited.
static int _hold(PySDL_Mixer *owner, _Held *held) {
    if(NULL == owner || NULL == owner->mixer) {
        _gone("Mixer");
        return 0;
    }
    held->owner = (PySDL_Mixer *)Py_NewRef((PyObject *)owner);
    held->mixer = owner->mixer;
    owner->busy++;
    Py_BEGIN_ALLOW_THREADS
        MIX_LockMixer(held->mixer);
    Py_END_ALLOW_THREADS
    return 1;
}

static void _release(_Held *held) {
    MIX_UnlockMixer(held->mixer);
    held->owner->busy--;
    Py_DECREF(held->owner);
}

// Hold a track's mixer and return the track, or NULL with an exception.
static MIX_Track * _hold_track(PySDL_MixerTrack *self, _Held *held, int mutates) {
    if(NULL == self->track) {
        _gone("MixerTrack");
        return NULL;
    }
    if(mutates && _tls_pcm) {
        PyErr_SetString(pysdl_Error, "a track cannot be changed from a raw / cooked mix callback");
        return NULL;
    }
    if(!_hold(self->owner, held)) {
        return NULL;
    }
    if(NULL == self->track) {
        _release(held);
        _gone("MixerTrack");
        return NULL;
    }
    return self->track;
}

static MIX_Group * _hold_group(PySDL_MixerGroup *self, _Held *held) {
    if(NULL == self->group) {
        _gone("MixerGroup");
        return NULL;
    }
    if(!_hold(self->owner, held)) {
        return NULL;
    }
    if(NULL == self->group) {
        _release(held);
        _gone("MixerGroup");
        return NULL;
    }
    return self->group;
}

// Null every Track / Group wrapper of `mixer` (all of them for NULL) and drop
// their callbacks: SDL is about to free them with the mixer. Their inputs move
// to `keep`, which the caller releases only once SDL has freed the mixer: it
// still touches a track's AudioStream while stopping it. The pointers go
// first; releasing callables can run arbitrary code (and drop the GIL), which
// may reshape the registries, so that comes after.
static void _keep(PyObject *keep, PyObject **input) {
    if(NULL == keep || 0 > PyList_Append(keep, *input)) {
        PyErr_Clear();  // out of memory: leak it rather than free it too early
        Py_INCREF(*input);
    }
    Py_CLEAR(*input);
}

static void _orphan(PySDL_Mixer *mixer, PyObject *keep) {
    PyObject *garbage = PyList_New(0);  // tokens to drop
    for(Py_ssize_t idx = 0; idx < _tracks.len; ++idx) {
        PySDL_MixerTrack *track = (PySDL_MixerTrack *)_tracks.items[idx];
        if(NULL != track->track && (NULL == mixer || track->owner == mixer)) {
            track->track = NULL;
            PyObject *token = PyLong_FromUnsignedLongLong(track->token);
            if(NULL == garbage || NULL == token || 0 > PyList_Append(garbage, token)) {
                PyErr_Clear();
            }
            Py_XDECREF(token);
            if(NULL != track->input) {
                _keep(keep, &track->input);
            }
        }
    }
    for(Py_ssize_t idx = 0; idx < _groups.len; ++idx) {
        PySDL_MixerGroup *group = (PySDL_MixerGroup *)_groups.items[idx];
        if(NULL != group->group && (NULL == mixer || group->owner == mixer)) {
            group->group = NULL;
            PyObject *token = PyLong_FromUnsignedLongLong(group->token);
            if(NULL == garbage || NULL == token || 0 > PyList_Append(garbage, token)) {
                PyErr_Clear();
            }
            Py_XDECREF(token);
        }
    }
    for(Py_ssize_t idx = 0; garbage && idx < PyList_GET_SIZE(garbage); ++idx) {
        _slots_drop(PyLong_AsUnsignedLongLong(PyList_GET_ITEM(garbage, idx)));
    }
    Py_XDECREF(garbage);
}

// Stop new users of `self`, then wait (GIL released) for the current ones.
// Returns the MIX_Mixer to free, or NULL if there was none; its tracks'
// inputs go to `keep` (see _orphan).
static MIX_Mixer * _retire(PySDL_Mixer *self, PyObject *keep) {
    MIX_Mixer *mixer = self->mixer;
    if(NULL == mixer) {
        return NULL;
    }
    self->mixer = NULL;
    _orphan(self, keep);
    _slots_drop(self->token);
    while(0 < self->busy) {
        Py_BEGIN_ALLOW_THREADS
            SDL_Delay(1);
        Py_END_ALLOW_THREADS
    }
    return mixer;
}

// A device mixer must be freed on the main thread and never from its own
// mixing thread; a wrapper dropped there hands the free to the main thread
// (Py_AddPendingCall). MIX_Quit frees every mixer itself, so it empties
// _pending and a queued free that finds its mixer gone does nothing.
static MIX_Mixer **_pending;
static Py_ssize_t _npending, _cappending;

static int _destroy_later(void *arg) {
    for(Py_ssize_t idx = 0; idx < _npending; ++idx) {
        if(_pending[idx] == arg) {
            _pending[idx] = _pending[--_npending];
            Py_BEGIN_ALLOW_THREADS
                MIX_DestroyMixer((MIX_Mixer *)arg);
            Py_END_ALLOW_THREADS
            break;
        }
    }
    return 0;
}

static void _destroy_mixer(MIX_Mixer *mixer, int device) {
    if(_tls_callback || (device && !SDL_IsMainThread())) {
        if(_npending == _cappending) {
            Py_ssize_t cap = _cappending ? _cappending * 2 : 8;
            MIX_Mixer **grown = PyMem_Realloc(_pending, cap * sizeof(MIX_Mixer *));
            if(NULL == grown) {
                return;  // out of memory: MIX_Quit will still free it
            }
            _pending = grown;
            _cappending = cap;
        }
        _pending[_npending++] = mixer;
        if(0 != Py_AddPendingCall(_destroy_later, mixer)) {
            --_npending;  // queue full: MIX_Quit will free it
        }
        return;
    }
    Py_BEGIN_ALLOW_THREADS
        MIX_DestroyMixer(mixer);
    Py_END_ALLOW_THREADS
}

//=========================================================
// callback trampolines (mixing thread)
//=========================================================

// The live wrapper of a registry with `token`, as a new reference, or None.
static PyObject * _by_token(PySDL_Registry *reg, unsigned long long token, size_t offset) {
    for(Py_ssize_t idx = 0; idx < reg->len; ++idx) {
        PyObject *obj = reg->items[idx];
        if(*(unsigned long long *)((char *)obj + offset) == token) {
            return Py_NewRef(obj);
        }
    }
    return Py_NewRef(Py_None);
}

#define TRACK_OF(token) _by_token(&_tracks, (token), offsetof(PySDL_MixerTrack, token))
#define GROUP_OF(token) _by_token(&_groups, (token), offsetof(PySDL_MixerGroup, token))
#define MIXER_OF(token) _by_token(&_mixers, (token), offsetof(PySDL_Mixer, token))

// Run fn(obj, ...) for a mixing-thread callback; any exception is printed.
// SDL may call these from a dealloc while an exception propagates (stopping a
// track), so a pending one is set aside meanwhile.
typedef struct {
    PyGILState_STATE gil;
    PyObject *et, *ev, *etb;
} _Enter;

static int _cb_enter(_Enter *enter) {
    if(!PySDL_ThreadEnter(&enter->gil)) {
        return 0;
    }
    PyErr_Fetch(&enter->et, &enter->ev, &enter->etb);
    _tls_callback++;
    return 1;
}

static void _cb_leave(_Enter *enter) {
    if(PyErr_Occurred()) {
        PyErr_Print();
    }
    _tls_callback--;
    PyErr_Restore(enter->et, enter->ev, enter->etb);
    PySDL_ThreadLeave(enter->gil);
}

static void SDLCALL _track_stopped(void *userdata, MIX_Track *track) {
    (void)track;
    _Enter enter;
    if(!_cb_enter(&enter)) {
        return;
    }
    unsigned long long token = (unsigned long long)(uintptr_t)userdata;
    PyObject *fn = _slot_get(token, SLOT_STOPPED);
    if(NULL != fn) {
        PyObject *obj = TRACK_OF(token);
        PyObject *result = PyObject_CallFunctionObjArgs(fn, obj, NULL);
        Py_XDECREF(result);
        Py_DECREF(obj);
        Py_DECREF(fn);
    }
    _cb_leave(&enter);
}

// fn(obj, spec, pcm_bytes) may return a same-sized bytes-like to replace the
// float32 samples, or None to leave them.
static void _pcm_call(PyObject *fn, PyObject *obj, const SDL_AudioSpec *spec, float *pcm, int samples) {
    Py_ssize_t size = (Py_ssize_t)samples * (Py_ssize_t)sizeof(float);
    PyObject *data = PyBytes_FromStringAndSize((const char *)pcm, 0 < size ? size : 0);
    PyObject *pyspec = _spec_to_py(spec);
    PyObject *result = (data && pyspec) ? PyObject_CallFunctionObjArgs(fn, obj, pyspec, data, NULL) : NULL;
    if(NULL != result && Py_None != result) {
        Py_buffer view;
        if(0 == PyObject_GetBuffer(result, &view, PyBUF_SIMPLE)) {
            if(view.len == size) {
                memcpy(pcm, view.buf, (size_t)size);
            } else {
                PyErr_Format(PyExc_ValueError, "mix callback returned %zd bytes for %zd bytes of samples",
                    view.len, size);
            }
            PyBuffer_Release(&view);
        }
    }
    Py_XDECREF(result);
    Py_XDECREF(pyspec);
    Py_XDECREF(data);
}

static void _track_pcm(void *userdata, const SDL_AudioSpec *spec, float *pcm, int samples, int slot) {
    _Enter enter;
    if(!_cb_enter(&enter)) {
        return;
    }
    unsigned long long token = (unsigned long long)(uintptr_t)userdata;
    PyObject *fn = _slot_get(token, slot);
    if(NULL != fn) {
        PyObject *obj = TRACK_OF(token);
        _tls_pcm++;
        _pcm_call(fn, obj, spec, pcm, samples);
        _tls_pcm--;
        Py_DECREF(obj);
        Py_DECREF(fn);
    }
    _cb_leave(&enter);
}

static void SDLCALL _track_raw(void *userdata, MIX_Track *track, const SDL_AudioSpec *spec, float *pcm, int samples) {
    (void)track;
    _track_pcm(userdata, spec, pcm, samples, SLOT_RAW);
}

static void SDLCALL _track_cooked(void *userdata, MIX_Track *track, const SDL_AudioSpec *spec, float *pcm, int samples) {
    (void)track;
    _track_pcm(userdata, spec, pcm, samples, SLOT_COOKED);
}

static void SDLCALL _group_post_mix(void *userdata, MIX_Group *group, const SDL_AudioSpec *spec, float *pcm, int samples) {
    (void)group;
    _Enter enter;
    if(!_cb_enter(&enter)) {
        return;
    }
    unsigned long long token = (unsigned long long)(uintptr_t)userdata;
    PyObject *fn = _slot_get(token, 0);
    if(NULL != fn) {
        PyObject *obj = GROUP_OF(token);
        _pcm_call(fn, obj, spec, pcm, samples);
        Py_DECREF(obj);
        Py_DECREF(fn);
    }
    _cb_leave(&enter);
}

static void SDLCALL _mixer_post_mix(void *userdata, MIX_Mixer *mixer, const SDL_AudioSpec *spec, float *pcm, int samples) {
    (void)mixer;
    _Enter enter;
    if(!_cb_enter(&enter)) {
        return;
    }
    unsigned long long token = (unsigned long long)(uintptr_t)userdata;
    PyObject *fn = _slot_get(token, 0);
    if(NULL != fn) {
        PyObject *obj = MIXER_OF(token);
        _pcm_call(fn, obj, spec, pcm, samples);
        Py_DECREF(obj);  // a mixer dropped here frees itself later, on the main thread
        Py_DECREF(fn);
    }
    _cb_leave(&enter);
}

// Install / clear one callback slot while holding the object's mixer.
#define SET_CALLBACK(setter, object, tramp, token, slot, nslots, fn)                     \
    do {                                                                                 \
        if(Py_None != (fn) && !PyCallable_Check(fn)) {                                   \
            PyErr_SetString(PyExc_TypeError, "the callback must be callable or None");   \
            goto fail;                                                                   \
        }                                                                                \
        if(!_slot_set((token), (slot), (nslots), (fn))) {                                \
            goto fail;                                                                   \
        }                                                                                \
        if(!setter((object), Py_None == (fn) ? NULL : (tramp), (void *)(uintptr_t)(token))) { \
            _raise();                                                                    \
            goto fail;                                                                   \
        }                                                                                \
    } while(0)

//=========================================================
// play options
//=========================================================

typedef struct {
    const char *name;   // keyword
    const char *prop;   // MIX_PROP_PLAY_*
    char kind;          // 'i' number, 'f' float, 'b' boolean
} _PlayOption;

static const _PlayOption _play_options[] = {
    { "loops",                 MIX_PROP_PLAY_LOOPS_NUMBER,                      'i' },
    { "max_frame",             MIX_PROP_PLAY_MAX_FRAME_NUMBER,                  'i' },
    { "max_ms",                MIX_PROP_PLAY_MAX_MILLISECONDS_NUMBER,           'i' },
    { "start_frame",           MIX_PROP_PLAY_START_FRAME_NUMBER,                'i' },
    { "start_ms",              MIX_PROP_PLAY_START_MILLISECOND_NUMBER,          'i' },
#if SDL_MIXER_VERSION_ATLEAST(3, 2, 2)
    { "start_order",           MIX_PROP_PLAY_START_ORDER_NUMBER,                'i' },
#endif
    { "loop_start_frame",      MIX_PROP_PLAY_LOOP_START_FRAME_NUMBER,           'i' },
    { "loop_start_ms",         MIX_PROP_PLAY_LOOP_START_MILLISECOND_NUMBER,     'i' },
    { "fade_in_frames",        MIX_PROP_PLAY_FADE_IN_FRAMES_NUMBER,             'i' },
    { "fade_in_ms",            MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER,       'i' },
    { "fade_in_start_gain",    MIX_PROP_PLAY_FADE_IN_START_GAIN_FLOAT,          'f' },
    { "append_silence_frames", MIX_PROP_PLAY_APPEND_SILENCE_FRAMES_NUMBER,      'i' },
    { "append_silence_ms",     MIX_PROP_PLAY_APPEND_SILENCE_MILLISECONDS_NUMBER,'i' },
    { "halt_when_exhausted",   MIX_PROP_PLAY_HALT_WHEN_EXHAUSTED_BOOLEAN,       'b' },
    { NULL }
};

// Keyword arguments -> a fresh properties set (0 when there are none), or
// (SDL_PropertiesID)-1 with an exception set.
static SDL_PropertiesID _play_props(PyObject *kwds) {
    if(NULL == kwds || 0 == PyDict_Size(kwds)) {
        return 0;
    }
    SDL_PropertiesID props = SDL_CreateProperties();
    if(0 == props) {
        _raise();
        return (SDL_PropertiesID)-1;
    }
    PyObject *key, *value;
    Py_ssize_t pos = 0;
    while(PyDict_Next(kwds, &pos, &key, &value)) {
        const char *name = PyUnicode_AsUTF8(key);
        const _PlayOption *opt = _play_options;
        while(name && opt->name && 0 != strcmp(opt->name, name)) {
            ++opt;
        }
        if(NULL == name || NULL == opt->name) {
            if(name) {
                PyErr_Format(PyExc_TypeError, "unknown play option '%s'", name);
            }
            goto fail;
        }
        bool ok;
        if('i' == opt->kind) {
            long long number = PyLong_AsLongLong(value);
            if(-1 == number && PyErr_Occurred()) {
                goto fail;
            }
            ok = SDL_SetNumberProperty(props, opt->prop, (Sint64)number);
        } else if('f' == opt->kind) {
            double number = PyFloat_AsDouble(value);
            if(-1.0 == number && PyErr_Occurred()) {
                goto fail;
            }
            ok = SDL_SetFloatProperty(props, opt->prop, (float)number);
        } else {
            int truth = PyObject_IsTrue(value);
            if(0 > truth) {
                goto fail;
            }
            ok = SDL_SetBooleanProperty(props, opt->prop, truth ? true : false);
        }
        if(!ok) {
            _raise();
            goto fail;
        }
    }
    return props;

fail:
    SDL_DestroyProperties(props);
    return (SDL_PropertiesID)-1;
}

//=========================================================
// module functions
//=========================================================

static PyObject * PySDL_MIX_Version(PyObject *self, PyObject *ign) {
    int v = MIX_Version();
    return Py_BuildValue("(iii)", SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v), SDL_VERSIONNUM_MICRO(v));
}

static PyObject * PySDL_MIX_Init(PyObject *self, PyObject *ign) {
    if(!_not_in_callback("MIX_Init")) {
        return NULL;
    }
    if(!MIX_Init()) {
        return _raise();
    }
    _inits++;
    Py_RETURN_NONE;
}

// Everything SDL_mixer frees on its final MIX_Quit: null all the wrappers.
// Waiting for users drops the GIL, so the registries are rescanned each time.
// Track inputs go to `keep`, released by the caller after MIX_Quit.
static void _teardown(PyObject *keep) {
    // This thread's own Lock()s: it would wait on itself. Their references move
    // to `unlocked` (released at the end) so no mixer is freed mid-scan.
    unsigned long me = PyThread_get_thread_ident();
    PyObject *unlocked = PyList_New(0);
    for(Py_ssize_t idx = 0; idx < _mixers.len; ++idx) {
        PySDL_Mixer *mixer = (PySDL_Mixer *)_mixers.items[idx];
        while(0 < mixer->locks && mixer->lock_thread == me) {
            mixer->locks--;
            MIX_UnlockMixer(mixer->locked_mixer);
            mixer->busy--;
            if(NULL != unlocked && 0 == PyList_Append(unlocked, (PyObject *)mixer)) {
                Py_DECREF(mixer);  // Lock()'s reference, now held by the list
            } else {
                PyErr_Clear();     // out of memory: keep Lock()'s reference (a leak)
            }
        }
    }
    for(;;) {
        PySDL_Mixer *live = NULL;
        for(Py_ssize_t idx = 0; idx < _mixers.len && NULL == live; ++idx) {
            if(NULL != ((PySDL_Mixer *)_mixers.items[idx])->mixer) {
                live = (PySDL_Mixer *)_mixers.items[idx];
            }
        }
        if(NULL == live) {
            break;
        }
        Py_INCREF(live);
        _retire(live, keep);  // SDL frees the MIX_Mixer itself in MIX_Quit
        Py_DECREF(live);
    }
    _orphan(NULL, keep);
    for(Py_ssize_t idx = 0; idx < _audios.len; ++idx) {
        ((PySDL_MixerAudio *)_audios.items[idx])->audio = NULL;
    }
    for(;;) {
        PySDL_MixerDecoder *busy = NULL;
        for(Py_ssize_t idx = 0; idx < _decoders.len && NULL == busy; ++idx) {
            PySDL_MixerDecoder *decoder = (PySDL_MixerDecoder *)_decoders.items[idx];
            if(decoder->busy) {
                busy = decoder;
            } else {
                decoder->decoder = NULL;
            }
        }
        if(NULL == busy) {
            break;
        }
        Py_BEGIN_ALLOW_THREADS
            SDL_Delay(1);
        Py_END_ALLOW_THREADS
    }
    _npending = 0;  // queued frees: MIX_Quit frees those mixers
    Py_CLEAR(_callbacks);
    Py_XDECREF(unlocked);
}

static void _quit_once(void) {
    PyObject *keep = NULL;
    if(1 == _inits) {
        keep = PyList_New(0);
        _teardown(keep);
    }
    _inits--;
    Py_BEGIN_ALLOW_THREADS
        MIX_Quit();  // the last one closes devices, joining their audio threads
    Py_END_ALLOW_THREADS
    Py_XDECREF(keep);  // track inputs, now that nothing reads them
}

static PyObject * PySDL_MIX_Quit(PyObject *self, PyObject *ign) {
    if(!_not_in_callback("MIX_Quit")) {
        return NULL;
    }
    if(0 < _inits) {
        _quit_once();
    }
    Py_RETURN_NONE;
}

void PySDL_MixerShutdown(void) {
    // SDL_Quit stops audio under SDL_mixer's feet: release it first.
    while(0 < _inits) {
        _quit_once();
    }
}

static PyObject * PySDL_MIX_GetNumAudioDecoders(PyObject *self, PyObject *ign) {
    return PyLong_FromLong(MIX_GetNumAudioDecoders());
}

static PyObject * PySDL_MIX_GetAudioDecoder(PyObject *self, PyObject *arg) {
    int index;
    if(!_as_int(arg, &index)) {
        return NULL;
    }
    const char *name = MIX_GetAudioDecoder(index);
    if(NULL == name) {
        Py_RETURN_NONE;
    }
    return PyUnicode_FromString(name);
}

static PyObject * PySDL_MIX_GetAudioDecoders(PyObject *self, PyObject *ign) {
    int count = MIX_GetNumAudioDecoders();
    PyObject *list = PyList_New(0);
    for(int idx = 0; list && idx < count; ++idx) {
        const char *name = MIX_GetAudioDecoder(idx);
        PyObject *str = name ? PyUnicode_FromString(name) : NULL;
        if(NULL == str || 0 > PyList_Append(list, str)) {
            Py_XDECREF(str);
            Py_CLEAR(list);
            break;
        }
        Py_DECREF(str);
    }
    return list;
}

static PyObject * PySDL_MIX_MSToFrames(PyObject *self, PyObject *args) {
    int rate;
    long long ms;
    if(!PyArg_ParseTuple(args, "iL", &rate, &ms)) {
        return NULL;
    }
    return PyLong_FromLongLong(MIX_MSToFrames(rate, ms));
}

static PyObject * PySDL_MIX_FramesToMS(PyObject *self, PyObject *args) {
    int rate;
    long long frames;
    if(!PyArg_ParseTuple(args, "iL", &rate, &frames)) {
        return NULL;
    }
    return PyLong_FromLongLong(MIX_FramesToMS(rate, frames));
}

// The MIX_Mixer of an optional `mixer` argument (None / omitted -> NULL), held
// busy so it outlives a GIL-released load. 0 with an exception on a bad one.
static int _preferred(PyObject *obj, PySDL_Mixer **owner) {
    *owner = NULL;
    if(NULL == obj || Py_None == obj) {
        return 1;
    }
    if(!PyObject_TypeCheck(obj, &PySDL_Mixer_Type)) {
        PyErr_Format(PyExc_TypeError, "mixer must be an SDL3.Mixer or None, not %.200s", Py_TYPE(obj)->tp_name);
        return 0;
    }
    if(NULL == ((PySDL_Mixer *)obj)->mixer) {
        _gone("Mixer");
        return 0;
    }
    *owner = (PySDL_Mixer *)Py_NewRef(obj);
    (*owner)->busy++;
    return 1;
}

static void _preferred_done(PySDL_Mixer *owner) {
    if(NULL != owner) {
        owner->busy--;
        Py_DECREF(owner);
    }
}

static PyObject * _wrap_audio(MIX_Audio *audio) {
    if(NULL == audio) {
        return _raise();
    }
    PySDL_MixerAudio *wrapper = (PySDL_MixerAudio *)PySDL_New(&PySDL_MixerAudio_Type);
    if(NULL == wrapper) {
        MIX_DestroyAudio(audio);
        return NULL;
    }
    wrapper->audio = audio;
    return (PyObject *)wrapper;
}

static PyObject * PySDL_MIX_LoadRawAudio(PyObject *self, PyObject *args, PyObject *kwds) {
    Py_buffer data;
    PyObject *spec_py, *mixer_py = NULL;
    static char *kwlist[] = {"data", "spec", "mixer", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "y*O|O", kwlist, &data, &spec_py, &mixer_py)) {
        return NULL;
    }
    SDL_AudioSpec spec;
    PySDL_Mixer *owner = NULL;
    MIX_Audio *audio = NULL;
    if(_need_init() && _spec_from_py(spec_py, &spec) && _preferred(mixer_py, &owner)) {
        MIX_Mixer *mixer = owner ? owner->mixer : NULL;
        Py_BEGIN_ALLOW_THREADS
            audio = MIX_LoadRawAudio(mixer, data.buf, (size_t)data.len, &spec);  // copies
        Py_END_ALLOW_THREADS
        _preferred_done(owner);
        PyBuffer_Release(&data);
        return _wrap_audio(audio);
    }
    PyBuffer_Release(&data);
    return NULL;
}

static PyObject * PySDL_MIX_CreateSineWaveAudio(PyObject *self, PyObject *args, PyObject *kwds) {
    int hz;
    float amplitude;
    long long ms = -1;
    PyObject *mixer_py = NULL;
    static char *kwlist[] = {"hz", "amplitude", "ms", "mixer", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "if|LO", kwlist, &hz, &amplitude, &ms, &mixer_py)) {
        return NULL;
    }
    PySDL_Mixer *owner;
    if(!_need_init() || !_preferred(mixer_py, &owner)) {
        return NULL;
    }
    MIX_Audio *audio = MIX_CreateSineWaveAudio(owner ? owner->mixer : NULL, hz, amplitude, ms);
    _preferred_done(owner);
    return _wrap_audio(audio);
}

static PyMethodDef pysdl_mixer_methods[] = {
    { "MIX_Version",            PySDL_MIX_Version,              METH_NOARGS  },
    { "MIX_Init",               PySDL_MIX_Init,                 METH_NOARGS  },
    { "MIX_Quit",               PySDL_MIX_Quit,                 METH_NOARGS  },
    { "MIX_GetNumAudioDecoders", PySDL_MIX_GetNumAudioDecoders, METH_NOARGS  },
    { "MIX_GetAudioDecoder",    PySDL_MIX_GetAudioDecoder,      METH_O       },
    { "MIX_GetAudioDecoders",   PySDL_MIX_GetAudioDecoders,     METH_NOARGS  },
    { "MIX_MSToFrames",         PySDL_MIX_MSToFrames,           METH_VARARGS },
    { "MIX_FramesToMS",         PySDL_MIX_FramesToMS,           METH_VARARGS },
    { "MIX_LoadRawAudio",       (PyCFunction)PySDL_MIX_LoadRawAudio,       METH_VARARGS | METH_KEYWORDS },
    { "MIX_CreateSineWaveAudio", (PyCFunction)PySDL_MIX_CreateSineWaveAudio, METH_VARARGS | METH_KEYWORDS },
    { NULL }
};

//=========================================================
// SDL3.Mixer
//=========================================================

static int        PySDL_Mixer_Type_init    (PySDL_Mixer*, PyObject*, PyObject*);
static void       PySDL_Mixer_Type_dealloc (PySDL_Mixer*);

static PyObject * PySDL_Mixer_Destroy            (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_GetFormat          (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_GetDevice          (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_Lock               (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_Unlock             (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_CreateTrack        (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_CreateGroup        (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_LoadAudio          (PySDL_Mixer*, PyObject*, PyObject*);
static PyObject * PySDL_Mixer_PlayAudio          (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_PlayTag            (PySDL_Mixer*, PyObject*, PyObject*);
static PyObject * PySDL_Mixer_StopAllTracks      (PySDL_Mixer*, PyObject*, PyObject*);
static PyObject * PySDL_Mixer_StopTag            (PySDL_Mixer*, PyObject*, PyObject*);
static PyObject * PySDL_Mixer_PauseAllTracks     (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_ResumeAllTracks    (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_PauseTag           (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_ResumeTag          (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_SetGain            (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_GetGain            (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_SetTagGain         (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_SetFrequencyRatio  (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_GetFrequencyRatio  (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_GetTaggedTracks    (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_SetPostMixCallback (PySDL_Mixer*, PyObject*);
static PyObject * PySDL_Mixer_Generate           (PySDL_Mixer*, PyObject*);

static PyMethodDef PySDL_Mixer_methods[] = {
    { "Destroy",            (PyCFunction)PySDL_Mixer_Destroy,            METH_NOARGS  },
    { "GetFormat",          (PyCFunction)PySDL_Mixer_GetFormat,          METH_NOARGS  },
    { "GetDevice",          (PyCFunction)PySDL_Mixer_GetDevice,          METH_NOARGS  },
    { "Lock",               (PyCFunction)PySDL_Mixer_Lock,               METH_NOARGS  },
    { "Unlock",             (PyCFunction)PySDL_Mixer_Unlock,             METH_NOARGS  },
    { "CreateTrack",        (PyCFunction)PySDL_Mixer_CreateTrack,        METH_NOARGS  },
    { "CreateGroup",        (PyCFunction)PySDL_Mixer_CreateGroup,        METH_NOARGS  },
    { "LoadAudio",          (PyCFunction)PySDL_Mixer_LoadAudio,          METH_VARARGS | METH_KEYWORDS },
    { "PlayAudio",          (PyCFunction)PySDL_Mixer_PlayAudio,          METH_O       },
    { "PlayTag",            (PyCFunction)PySDL_Mixer_PlayTag,            METH_VARARGS | METH_KEYWORDS },
    { "StopAllTracks",      (PyCFunction)PySDL_Mixer_StopAllTracks,      METH_VARARGS | METH_KEYWORDS },
    { "StopTag",            (PyCFunction)PySDL_Mixer_StopTag,            METH_VARARGS | METH_KEYWORDS },
    { "PauseAllTracks",     (PyCFunction)PySDL_Mixer_PauseAllTracks,     METH_NOARGS  },
    { "ResumeAllTracks",    (PyCFunction)PySDL_Mixer_ResumeAllTracks,    METH_NOARGS  },
    { "PauseTag",           (PyCFunction)PySDL_Mixer_PauseTag,           METH_O       },
    { "ResumeTag",          (PyCFunction)PySDL_Mixer_ResumeTag,          METH_O       },
    { "SetGain",            (PyCFunction)PySDL_Mixer_SetGain,            METH_O       },
    { "GetGain",            (PyCFunction)PySDL_Mixer_GetGain,            METH_NOARGS  },
    { "SetTagGain",         (PyCFunction)PySDL_Mixer_SetTagGain,         METH_VARARGS },
    { "SetFrequencyRatio",  (PyCFunction)PySDL_Mixer_SetFrequencyRatio,  METH_O       },
    { "GetFrequencyRatio",  (PyCFunction)PySDL_Mixer_GetFrequencyRatio,  METH_NOARGS  },
    { "GetTaggedTracks",    (PyCFunction)PySDL_Mixer_GetTaggedTracks,    METH_O       },
    { "SetPostMixCallback", (PyCFunction)PySDL_Mixer_SetPostMixCallback, METH_O       },
    { "Generate",           (PyCFunction)PySDL_Mixer_Generate,           METH_O       },
    { NULL }
};

PyTypeObject PySDL_Mixer_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.Mixer",
    .tp_basicsize = sizeof(PySDL_Mixer),
    .tp_dealloc   = (destructor)PySDL_Mixer_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.Mixer(spec=None, device=AUDIO_DEVICE_DEFAULT_PLAYBACK) - MIX_Mixer on an "
                    "audio device; device=None makes an offline mixer (spec required) driven by Generate()",
    .tp_methods   = PySDL_Mixer_methods,
    .tp_init      = (initproc)PySDL_Mixer_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_Mixer_Type_init(PySDL_Mixer *self, PyObject *args, PyObject *kwds) {
    PyObject *spec_py = Py_None;
    PyObject *device_py = NULL;
    static char *kwlist[] = {"spec", "device", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|OO", kwlist, &spec_py, &device_py)) {
        return -1;
    }
    if(NULL != self->mixer) {
        PyErr_SetString(pysdl_Error, "Mixer is already initialized");
        return -1;
    }
    if(0 == self->token) {
        self->token = ++_next_token;
        if(0 > PySDL_RegistryAdd(&_mixers, (PyObject *)self)) {
            return -1;
        }
    }
    SDL_AudioSpec spec;
    SDL_AudioSpec *specp = NULL;
    if(Py_None != spec_py) {
        if(!_spec_from_py(spec_py, &spec)) {
            return -1;
        }
        specp = &spec;
    }
    if(!_need_init()) {
        return -1;
    }
    MIX_Mixer *mixer;
    if(NULL != device_py && Py_None == device_py) {
        if(NULL == specp) {
            PyErr_SetString(PyExc_ValueError, "an offline mixer (device=None) needs a spec");
            return -1;
        }
        mixer = MIX_CreateMixer(specp);
        self->device = 0;
    } else {
        SDL_AudioDeviceID devid = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
        if(NULL != device_py) {
            unsigned long value = PyLong_AsUnsignedLong(device_py);
            if((unsigned long)-1 == value && PyErr_Occurred()) {
                return -1;
            }
            devid = (SDL_AudioDeviceID)value;
        }
        Py_BEGIN_ALLOW_THREADS
            mixer = MIX_CreateMixerDevice(devid, specp);
        Py_END_ALLOW_THREADS
        self->device = 1;
    }
    if(NULL == mixer) {
        _raise();
        return -1;
    }
    self->mixer = mixer;
    return 0;
}

static void PySDL_Mixer_Type_dealloc(PySDL_Mixer *self) {
    PySDL_RegistryRemove(&_mixers, (PyObject *)self);
    // No Track / Group wrappers are left (they hold a reference), so there are
    // no inputs to keep past a deferred free.
    PyObject *keep = PyList_New(0);
    MIX_Mixer *mixer = _retire(self, keep);
    if(NULL != mixer) {
        _destroy_mixer(mixer, self->device);
    }
    Py_XDECREF(keep);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject * PySDL_Mixer_Destroy(PySDL_Mixer *self, PyObject *ign) {
    if(!_not_in_callback("Mixer.Destroy")) {
        return NULL;
    }
    if(0 < self->locks && self->lock_thread == PyThread_get_thread_ident()) {
        PyErr_SetString(pysdl_Error, "Unlock() the mixer before destroying it");
        return NULL;
    }
    if(self->device && NULL != self->mixer && !SDL_IsMainThread()) {
        PyErr_SetString(pysdl_Error, "a device Mixer must be destroyed on the main thread");
        return NULL;
    }
    PyObject *keep = PyList_New(0);
    MIX_Mixer *mixer = _retire(self, keep);
    if(NULL != mixer) {
        _destroy_mixer(mixer, self->device);  // here and now: not in a callback, on the main thread
    }
    Py_XDECREF(keep);  // track inputs, once SDL no longer reads them
    Py_RETURN_NONE;
}

static PyObject * PySDL_Mixer_GetFormat(PySDL_Mixer *self, PyObject *ign) {
    _Held held;
    if(!_hold(self, &held)) {
        return NULL;
    }
    SDL_AudioSpec spec;
    bool ok = MIX_GetMixerFormat(held.mixer, &spec);
    _release(&held);
    return ok ? _spec_to_py(&spec) : _raise();
}

static PyObject * PySDL_Mixer_GetDevice(PySDL_Mixer *self, PyObject *ign) {
    _Held held;
    if(!_hold(self, &held)) {
        return NULL;
    }
    Sint64 device = SDL_GetNumberProperty(MIX_GetMixerProperties(held.mixer), MIX_PROP_MIXER_DEVICE_NUMBER, 0);
    _release(&held);
    if(0 == device) {
        Py_RETURN_NONE;
    }
    return PyLong_FromLongLong(device);
}

// Lock() / Unlock() bracket several calls the mixing thread must see at once
// (e.g. starting tracks on the same sample frame). Same thread, paired.
static PyObject * PySDL_Mixer_Lock(PySDL_Mixer *self, PyObject *ign) {
    _Held held;
    if(!_hold(self, &held)) {
        return NULL;
    }
    // Keep the mutex, the busy count and the reference until Unlock().
    self->locks++;
    self->lock_thread = PyThread_get_thread_ident();
    self->locked_mixer = held.mixer;
    Py_RETURN_NONE;
}

static PyObject * PySDL_Mixer_Unlock(PySDL_Mixer *self, PyObject *ign) {
    if(0 == self->locks || self->lock_thread != PyThread_get_thread_ident()) {
        PyErr_SetString(pysdl_Error, "the mixer is not locked by this thread");
        return NULL;
    }
    // The MIX_Mixer is still there: busy > 0 keeps Destroy / MIX_Quit waiting.
    self->locks--;
    MIX_UnlockMixer(self->locked_mixer);
    self->busy--;
    Py_DECREF(self);  // Lock()'s reference
    Py_RETURN_NONE;
}

static PyObject * _new_track(PySDL_Mixer *owner);
static PyObject * _new_group(PySDL_Mixer *owner);

static PyObject * PySDL_Mixer_CreateTrack(PySDL_Mixer *self, PyObject *ign) {
    return _new_track(self);
}

static PyObject * PySDL_Mixer_CreateGroup(PySDL_Mixer *self, PyObject *ign) {
    return _new_group(self);
}

static PyObject * PySDL_Mixer_LoadAudio(PySDL_Mixer *self, PyObject *args, PyObject *kwds) {
    // Mixer.LoadAudio(src, predecode=False, ...) is MixerAudio(src, mixer=self, ...).
    Py_ssize_t nargs = PyTuple_GET_SIZE(args);
    if(1 > nargs) {
        PyErr_SetString(PyExc_TypeError, "LoadAudio() needs the audio source");
        return NULL;
    }
    PyObject *call_args = PyTuple_New(nargs + 1);
    if(NULL == call_args) {
        return NULL;
    }
    PyTuple_SET_ITEM(call_args, 0, Py_NewRef(PyTuple_GET_ITEM(args, 0)));
    PyTuple_SET_ITEM(call_args, 1, Py_NewRef((PyObject *)self));
    for(Py_ssize_t idx = 1; idx < nargs; ++idx) {
        PyTuple_SET_ITEM(call_args, idx + 1, Py_NewRef(PyTuple_GET_ITEM(args, idx)));
    }
    PyObject *audio = PyObject_Call((PyObject *)&PySDL_MixerAudio_Type, call_args, kwds);
    Py_DECREF(call_args);
    return audio;
}

static MIX_Audio * _audio_arg(PyObject *obj) {
    if(!PyObject_TypeCheck(obj, &PySDL_MixerAudio_Type)) {
        PyErr_Format(PyExc_TypeError, "expected an SDL3.MixerAudio, not %.200s", Py_TYPE(obj)->tp_name);
        return NULL;
    }
    if(NULL == ((PySDL_MixerAudio *)obj)->audio) {
        _gone("MixerAudio");
        return NULL;
    }
    return ((PySDL_MixerAudio *)obj)->audio;
}

static PyObject * PySDL_Mixer_PlayAudio(PySDL_Mixer *self, PyObject *arg) {
    _Held held;
    if(NULL == _audio_arg(arg) || !_hold(self, &held)) {
        return NULL;
    }
    MIX_Audio *audio = _audio_arg(arg);  // re-read: still ours, the GIL was only lent
    bool ok = (NULL != audio) && MIX_PlayAudio(held.mixer, audio);  // refcounts the audio
    _release(&held);
    if(NULL == audio) {
        return NULL;
    }
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Mixer_PlayTag(PySDL_Mixer *self, PyObject *args, PyObject *kwds) {
    const char *tag;
    if(!PyArg_ParseTuple(args, "s", &tag)) {
        return NULL;
    }
    SDL_PropertiesID props = _play_props(kwds);
    if((SDL_PropertiesID)-1 == props) {
        return NULL;
    }
    _Held held;
    if(!_hold(self, &held)) {
        SDL_DestroyProperties(props);
        return NULL;
    }
    bool ok = MIX_PlayTag(held.mixer, tag, props);
    _release(&held);
    if(0 != props) {
        SDL_DestroyProperties(props);
    }
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Mixer_StopAllTracks(PySDL_Mixer *self, PyObject *args, PyObject *kwds) {
    long long fade_out_ms = 0;
    static char *kwlist[] = {"fade_out_ms", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|L", kwlist, &fade_out_ms)) {
        return NULL;
    }
    _Held held;
    if(!_hold(self, &held)) {
        return NULL;
    }
    bool ok = MIX_StopAllTracks(held.mixer, fade_out_ms);
    _release(&held);
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Mixer_StopTag(PySDL_Mixer *self, PyObject *args, PyObject *kwds) {
    const char *tag;
    long long fade_out_ms = 0;
    static char *kwlist[] = {"tag", "fade_out_ms", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|L", kwlist, &tag, &fade_out_ms)) {
        return NULL;
    }
    _Held held;
    if(!_hold(self, &held)) {
        return NULL;
    }
    bool ok = MIX_StopTag(held.mixer, tag, fade_out_ms);
    _release(&held);
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

#define MIXER_CALL(name, call)                                          \
    static PyObject * name(PySDL_Mixer *self, PyObject *ign) {          \
        _Held held;                                                     \
        if(!_hold(self, &held)) {                                       \
            return NULL;                                                \
        }                                                               \
        bool ok = call(held.mixer);                                     \
        _release(&held);                                                \
        if(!ok) {                                                       \
            return _raise();                                            \
        }                                                               \
        Py_RETURN_NONE;                                                 \
    }

MIXER_CALL(PySDL_Mixer_PauseAllTracks,  MIX_PauseAllTracks)
MIXER_CALL(PySDL_Mixer_ResumeAllTracks, MIX_ResumeAllTracks)

#define MIXER_TAG_CALL(name, call)                                      \
    static PyObject * name(PySDL_Mixer *self, PyObject *arg) {          \
        const char *tag = PyUnicode_AsUTF8(arg);                        \
        _Held held;                                                     \
        if(NULL == tag || !_hold(self, &held)) {                        \
            return NULL;                                                \
        }                                                               \
        bool ok = call(held.mixer, tag);                                \
        _release(&held);                                                \
        if(!ok) {                                                       \
            return _raise();                                            \
        }                                                               \
        Py_RETURN_NONE;                                                 \
    }

MIXER_TAG_CALL(PySDL_Mixer_PauseTag,  MIX_PauseTag)
MIXER_TAG_CALL(PySDL_Mixer_ResumeTag, MIX_ResumeTag)

static PyObject * PySDL_Mixer_SetGain(PySDL_Mixer *self, PyObject *arg) {
    double gain = PyFloat_AsDouble(arg);
    _Held held;
    if((-1.0 == gain && PyErr_Occurred()) || !_hold(self, &held)) {
        return NULL;
    }
    bool ok = MIX_SetMixerGain(held.mixer, (float)gain);
    _release(&held);
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Mixer_GetGain(PySDL_Mixer *self, PyObject *ign) {
    _Held held;
    if(!_hold(self, &held)) {
        return NULL;
    }
    float gain = MIX_GetMixerGain(held.mixer);
    _release(&held);
    return PyFloat_FromDouble(gain);
}

static PyObject * PySDL_Mixer_SetTagGain(PySDL_Mixer *self, PyObject *args) {
    const char *tag;
    float gain;
    _Held held;
    if(!PyArg_ParseTuple(args, "sf", &tag, &gain) || !_hold(self, &held)) {
        return NULL;
    }
    bool ok = MIX_SetTagGain(held.mixer, tag, gain);
    _release(&held);
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Mixer_SetFrequencyRatio(PySDL_Mixer *self, PyObject *arg) {
    double ratio = PyFloat_AsDouble(arg);
    _Held held;
    if((-1.0 == ratio && PyErr_Occurred()) || !_hold(self, &held)) {
        return NULL;
    }
    bool ok = MIX_SetMixerFrequencyRatio(held.mixer, (float)ratio);
    _release(&held);
    if(!ok) {
        return _raise();
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_Mixer_GetFrequencyRatio(PySDL_Mixer *self, PyObject *ign) {
    _Held held;
    if(!_hold(self, &held)) {
        return NULL;
    }
    float ratio = MIX_GetMixerFrequencyRatio(held.mixer);
    _release(&held);
    return PyFloat_FromDouble(ratio);
}

// Only tracks with a wrapper are reported (MIX_PlayAudio's are internal).
static PyObject * PySDL_Mixer_GetTaggedTracks(PySDL_Mixer *self, PyObject *arg) {
    const char *tag = PyUnicode_AsUTF8(arg);
    _Held held;
    if(NULL == tag || !_hold(self, &held)) {
        return NULL;
    }
    int count = 0;
    MIX_Track **tracks = MIX_GetTaggedTracks(held.mixer, tag, &count);
    PyObject *list = NULL;
    if(NULL == tracks) {
        _raise();
    } else if(NULL != (list = PyList_New(0))) {
        for(int idx = 0; idx < count && list; ++idx) {
            for(Py_ssize_t jdx = 0; jdx < _tracks.len; ++jdx) {
                PySDL_MixerTrack *wrapper = (PySDL_MixerTrack *)_tracks.items[jdx];
                if(wrapper->track == tracks[idx]) {
                    if(0 > PyList_Append(list, (PyObject *)wrapper)) {
                        Py_CLEAR(list);
                    }
                    break;
                }
            }
        }
    }
    SDL_free(tracks);
    _release(&held);
    return list;
}

static PyObject * PySDL_Mixer_SetPostMixCallback(PySDL_Mixer *self, PyObject *fn) {
    _Held held;
    if(!_hold(self, &held)) {
        return NULL;
    }
    SET_CALLBACK(MIX_SetPostMixCallback, held.mixer, _mixer_post_mix, self->token, 0, 1, fn);
    _release(&held);
    Py_RETURN_NONE;
fail:
    _release(&held);
    return NULL;
}

// Mixes `nbytes` (a whole number of output frames) and returns (data, mixed):
// all of `data` is valid, `mixed` bytes of it real audio, the rest silence.
static PyObject * PySDL_Mixer_Generate(PySDL_Mixer *self, PyObject *arg) {
    long nbytes = PyLong_AsLong(arg);
    if(-1 == nbytes && PyErr_Occurred()) {
        return NULL;
    }
    if(0 > nbytes || nbytes > INT_MAX) {
        PyErr_SetString(PyExc_ValueError, "nbytes must be between 0 and INT_MAX");
        return NULL;
    }
    if(NULL == self->mixer) {
        return _gone("Mixer");
    }
    if(self->device) {
        PyErr_SetString(pysdl_Error, "Generate() needs an offline mixer (Mixer(spec, device=None))");
        return NULL;
    }
    if(0 == nbytes) {
        return Py_BuildValue("(y#i)", "", (Py_ssize_t)0, 0);  // SDL rejects an empty buffer
    }
    PyObject *data = PyBytes_FromStringAndSize(NULL, nbytes);
    if(NULL == data) {
        return NULL;
    }
    // The mixer locks itself and runs the callbacks on this thread; keep the
    // GIL free for them (and other threads) meanwhile.
    MIX_Mixer *mixer = self->mixer;
    self->busy++;
    Py_INCREF(self);
    int mixed;
    Py_BEGIN_ALLOW_THREADS
        mixed = MIX_Generate(mixer, PyBytes_AS_STRING(data), (int)nbytes);
    Py_END_ALLOW_THREADS
    self->busy--;
    Py_DECREF(self);
    if(0 > mixed) {
        Py_DECREF(data);
        return _raise();
    }
    return Py_BuildValue("(Ni)", data, mixed);
}

//=========================================================
// SDL3.MixerAudio
//=========================================================

static int        PySDL_MixerAudio_Type_init    (PySDL_MixerAudio*, PyObject*, PyObject*);
static void       PySDL_MixerAudio_Type_dealloc (PySDL_MixerAudio*);

static PyObject * PySDL_MixerAudio_Destroy     (PySDL_MixerAudio*, PyObject*);
static PyObject * PySDL_MixerAudio_GetDuration (PySDL_MixerAudio*, PyObject*);
static PyObject * PySDL_MixerAudio_GetFormat   (PySDL_MixerAudio*, PyObject*);
static PyObject * PySDL_MixerAudio_GetMetadata (PySDL_MixerAudio*, PyObject*);
static PyObject * PySDL_MixerAudio_MSToFrames  (PySDL_MixerAudio*, PyObject*);
static PyObject * PySDL_MixerAudio_FramesToMS  (PySDL_MixerAudio*, PyObject*);

static PyMethodDef PySDL_MixerAudio_methods[] = {
    { "Destroy",     (PyCFunction)PySDL_MixerAudio_Destroy,     METH_NOARGS },
    { "GetDuration", (PyCFunction)PySDL_MixerAudio_GetDuration, METH_NOARGS },
    { "GetFormat",   (PyCFunction)PySDL_MixerAudio_GetFormat,   METH_NOARGS },
    { "GetMetadata", (PyCFunction)PySDL_MixerAudio_GetMetadata, METH_NOARGS },
    { "MSToFrames",  (PyCFunction)PySDL_MixerAudio_MSToFrames,  METH_O      },
    { "FramesToMS",  (PyCFunction)PySDL_MixerAudio_FramesToMS,  METH_O      },
    { NULL }
};

PyTypeObject PySDL_MixerAudio_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.MixerAudio",
    .tp_basicsize = sizeof(PySDL_MixerAudio),
    .tp_dealloc   = (destructor)PySDL_MixerAudio_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.MixerAudio(src, mixer=None, predecode=False, *, decoder=None, "
                    "skip_metadata=False, ignore_loops=False) - MIX_Audio from a path or the file's bytes",
    .tp_methods   = PySDL_MixerAudio_methods,
    .tp_init      = (initproc)PySDL_MixerAudio_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_MixerAudio_Type_init(PySDL_MixerAudio *self, PyObject *args, PyObject *kwds) {
    PyObject *src = NULL, *mixer_py = NULL;
    int predecode = 0, skip_metadata = 0, ignore_loops = 0;
    const char *decoder = NULL;
    static char *kwlist[] = {"src", "mixer", "predecode", "decoder", "skip_metadata", "ignore_loops", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|OOp$zpp", kwlist,
        &src, &mixer_py, &predecode, &decoder, &skip_metadata, &ignore_loops)) {
        return -1;
    }
    if(0 > PySDL_RegistryAdd(&_audios, (PyObject *)self)) {
        return -1;
    }
    if(NULL == src) {
        return 0;  // internal allocation: the C side fills ->audio in
    }
    if(NULL != self->audio) {
        PyErr_SetString(pysdl_Error, "MixerAudio is already initialized");
        return -1;
    }
#if !SDL_MIXER_VERSION_ATLEAST(3, 2, 4)
    if(ignore_loops) {
        PyErr_SetString(pysdl_Error, "ignore_loops needs SDL_mixer 3.2.4 or newer");
        return -1;
    }
#endif
    if(!_need_init()) {
        return -1;
    }

    // SDL_mixer reads (and caches) all of it during the load, so `view` only
    // has to pin a bytes source for the call.
    Py_buffer view;
    SDL_IOStream *io = PySDL_IOFromObject(src, &view);
    if(NULL == io) {
        return -1;
    }
    PySDL_Mixer *owner;
    if(!_preferred(mixer_py, &owner)) {
        SDL_CloseIO(io);
        PyBuffer_Release(&view);
        return -1;
    }
    MIX_Audio *audio = NULL;
    SDL_PropertiesID props = SDL_CreateProperties();
    if(0 != props
        && SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_IOSTREAM_POINTER, io)
        && SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_CLOSEIO_BOOLEAN, true)
        && SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_PREDECODE_BOOLEAN, predecode ? true : false)
        && SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_PREFERRED_MIXER_POINTER, owner ? owner->mixer : NULL)
        && SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_SKIP_METADATA_TAGS_BOOLEAN, skip_metadata ? true : false)
#if SDL_MIXER_VERSION_ATLEAST(3, 2, 4)
        && SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_IGNORE_LOOPS_BOOLEAN, ignore_loops ? true : false)
#endif
        && (NULL == decoder || SDL_SetStringProperty(props, MIX_PROP_AUDIO_DECODER_STRING, decoder))) {
        Py_BEGIN_ALLOW_THREADS
            audio = MIX_LoadAudioWithProperties(props);  // closes `io`
        Py_END_ALLOW_THREADS
    } else {
        SDL_CloseIO(io);
    }
    if(NULL == audio) {
        PyErr_SetString(pysdl_Error, SDL_GetError());
    }
    SDL_DestroyProperties(props);
    _preferred_done(owner);
    PyBuffer_Release(&view);
    if(NULL == audio) {
        return -1;
    }
    self->audio = audio;
    return 0;
}

static void _audio_destroy(PySDL_MixerAudio *self) {
    MIX_Audio *audio = self->audio;
    if(NULL != audio) {
        self->audio = NULL;
        Py_BEGIN_ALLOW_THREADS
            MIX_DestroyAudio(audio);  // refcounted: tracks still using it keep it
        Py_END_ALLOW_THREADS
    }
}

static void PySDL_MixerAudio_Type_dealloc(PySDL_MixerAudio *self) {
    PySDL_RegistryRemove(&_audios, (PyObject *)self);
    _audio_destroy(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject * PySDL_MixerAudio_Destroy(PySDL_MixerAudio *self, PyObject *ign) {
    _audio_destroy(self);
    Py_RETURN_NONE;
}

static PyObject * PySDL_MixerAudio_GetDuration(PySDL_MixerAudio *self, PyObject *ign) {
    if(NULL == self->audio) {
        return _gone("MixerAudio");
    }
    return PyLong_FromLongLong(MIX_GetAudioDuration(self->audio));
}

static PyObject * PySDL_MixerAudio_GetFormat(PySDL_MixerAudio *self, PyObject *ign) {
    if(NULL == self->audio) {
        return _gone("MixerAudio");
    }
    SDL_AudioSpec spec;
    if(!MIX_GetAudioFormat(self->audio, &spec)) {
        return _raise();
    }
    return _spec_to_py(&spec);
}

// The metadata SDL_mixer found, as a dict of the keys present.
static PyObject * PySDL_MixerAudio_GetMetadata(PySDL_MixerAudio *self, PyObject *ign) {
    if(NULL == self->audio) {
        return _gone("MixerAudio");
    }
    static const struct { const char *key; const char *prop; } fields[] = {
        { "title",             MIX_PROP_METADATA_TITLE_STRING },
        { "artist",            MIX_PROP_METADATA_ARTIST_STRING },
        { "album",             MIX_PROP_METADATA_ALBUM_STRING },
        { "copyright",         MIX_PROP_METADATA_COPYRIGHT_STRING },
        { "track",             MIX_PROP_METADATA_TRACK_NUMBER },
        { "total_tracks",      MIX_PROP_METADATA_TOTAL_TRACKS_NUMBER },
        { "year",              MIX_PROP_METADATA_YEAR_NUMBER },
        { "duration_frames",   MIX_PROP_METADATA_DURATION_FRAMES_NUMBER },
        { "duration_infinite", MIX_PROP_METADATA_DURATION_INFINITE_BOOLEAN },
    };
    SDL_PropertiesID props = MIX_GetAudioProperties(self->audio);
    if(0 == props) {
        return _raise();
    }
    PyObject *dict = PyDict_New();
    for(size_t idx = 0; dict && idx < SDL_arraysize(fields); ++idx) {
        PyObject *value = NULL;
        switch(SDL_GetPropertyType(props, fields[idx].prop)) {
        case SDL_PROPERTY_TYPE_STRING: {
                const char *str = SDL_GetStringProperty(props, fields[idx].prop, NULL);
                value = str ? PyUnicode_DecodeUTF8(str, (Py_ssize_t)strlen(str), "replace") : NULL;
            }
            break;
        case SDL_PROPERTY_TYPE_NUMBER:
            value = PyLong_FromLongLong(SDL_GetNumberProperty(props, fields[idx].prop, 0));
            break;
        case SDL_PROPERTY_TYPE_BOOLEAN:
            value = PyBool_FromLong(SDL_GetBooleanProperty(props, fields[idx].prop, false));
            break;
        default:
            continue;
        }
        if(NULL == value || 0 > PyDict_SetItemString(dict, fields[idx].key, value)) {
            Py_CLEAR(dict);
        }
        Py_XDECREF(value);
    }
    return dict;
}

static PyObject * PySDL_MixerAudio_MSToFrames(PySDL_MixerAudio *self, PyObject *arg) {
    long long ms = PyLong_AsLongLong(arg);
    if(-1 == ms && PyErr_Occurred()) {
        return NULL;
    }
    if(NULL == self->audio) {
        return _gone("MixerAudio");
    }
    return PyLong_FromLongLong(MIX_AudioMSToFrames(self->audio, ms));
}

static PyObject * PySDL_MixerAudio_FramesToMS(PySDL_MixerAudio *self, PyObject *arg) {
    long long frames = PyLong_AsLongLong(arg);
    if(-1 == frames && PyErr_Occurred()) {
        return NULL;
    }
    if(NULL == self->audio) {
        return _gone("MixerAudio");
    }
    return PyLong_FromLongLong(MIX_AudioFramesToMS(self->audio, frames));
}

//=========================================================
// SDL3.MixerTrack
//=========================================================

static int        PySDL_MixerTrack_Type_init    (PySDL_MixerTrack*, PyObject*, PyObject*);
static void       PySDL_MixerTrack_Type_dealloc (PySDL_MixerTrack*);

static PyObject * PySDL_MixerTrack_Destroy              (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_GetMixer             (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetAudio             (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_GetAudio             (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetAudioStream       (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_GetAudioStream       (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetIOStream          (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetRawIOStream       (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_Tag                  (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_Untag                (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_GetTags              (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetPlaybackPosition  (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_GetPlaybackPosition  (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_GetFadeFrames        (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_GetLoops             (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetLoops             (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_GetRemaining         (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_MSToFrames           (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_FramesToMS           (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_Play                 (PySDL_MixerTrack*, PyObject*, PyObject*);
static PyObject * PySDL_MixerTrack_Stop                 (PySDL_MixerTrack*, PyObject*, PyObject*);
static PyObject * PySDL_MixerTrack_Pause                (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_Resume               (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_Playing              (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_Paused               (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetGain              (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_GetGain              (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetFrequencyRatio    (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_GetFrequencyRatio    (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetOutputChannelMap  (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetStereo            (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_Set3DPosition        (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_Get3DPosition        (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetGroup             (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetStoppedCallback   (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetRawCallback       (PySDL_MixerTrack*, PyObject*);
static PyObject * PySDL_MixerTrack_SetCookedCallback    (PySDL_MixerTrack*, PyObject*);

static PyMethodDef PySDL_MixerTrack_methods[] = {
    { "Destroy",             (PyCFunction)PySDL_MixerTrack_Destroy,             METH_NOARGS  },
    { "GetMixer",            (PyCFunction)PySDL_MixerTrack_GetMixer,            METH_NOARGS  },
    { "SetAudio",            (PyCFunction)PySDL_MixerTrack_SetAudio,            METH_O       },
    { "GetAudio",            (PyCFunction)PySDL_MixerTrack_GetAudio,            METH_NOARGS  },
    { "SetAudioStream",      (PyCFunction)PySDL_MixerTrack_SetAudioStream,      METH_O       },
    { "GetAudioStream",      (PyCFunction)PySDL_MixerTrack_GetAudioStream,      METH_NOARGS  },
    { "SetIOStream",         (PyCFunction)PySDL_MixerTrack_SetIOStream,         METH_O       },
    { "SetRawIOStream",      (PyCFunction)PySDL_MixerTrack_SetRawIOStream,      METH_VARARGS },
    { "Tag",                 (PyCFunction)PySDL_MixerTrack_Tag,                 METH_O       },
    { "Untag",               (PyCFunction)PySDL_MixerTrack_Untag,               METH_O       },
    { "GetTags",             (PyCFunction)PySDL_MixerTrack_GetTags,             METH_NOARGS  },
    { "SetPlaybackPosition", (PyCFunction)PySDL_MixerTrack_SetPlaybackPosition, METH_O       },
    { "GetPlaybackPosition", (PyCFunction)PySDL_MixerTrack_GetPlaybackPosition, METH_NOARGS  },
    { "GetFadeFrames",       (PyCFunction)PySDL_MixerTrack_GetFadeFrames,       METH_NOARGS  },
    { "GetLoops",            (PyCFunction)PySDL_MixerTrack_GetLoops,            METH_NOARGS  },
    { "SetLoops",            (PyCFunction)PySDL_MixerTrack_SetLoops,            METH_O       },
    { "GetRemaining",        (PyCFunction)PySDL_MixerTrack_GetRemaining,        METH_NOARGS  },
    { "MSToFrames",          (PyCFunction)PySDL_MixerTrack_MSToFrames,          METH_O       },
    { "FramesToMS",          (PyCFunction)PySDL_MixerTrack_FramesToMS,          METH_O       },
    { "Play",                (PyCFunction)PySDL_MixerTrack_Play,                METH_VARARGS | METH_KEYWORDS },
    { "Stop",                (PyCFunction)PySDL_MixerTrack_Stop,                METH_VARARGS | METH_KEYWORDS },
    { "Pause",               (PyCFunction)PySDL_MixerTrack_Pause,               METH_NOARGS  },
    { "Resume",              (PyCFunction)PySDL_MixerTrack_Resume,              METH_NOARGS  },
    { "Playing",             (PyCFunction)PySDL_MixerTrack_Playing,             METH_NOARGS  },
    { "Paused",              (PyCFunction)PySDL_MixerTrack_Paused,              METH_NOARGS  },
    { "SetGain",             (PyCFunction)PySDL_MixerTrack_SetGain,             METH_O       },
    { "GetGain",             (PyCFunction)PySDL_MixerTrack_GetGain,             METH_NOARGS  },
    { "SetFrequencyRatio",   (PyCFunction)PySDL_MixerTrack_SetFrequencyRatio,   METH_O       },
    { "GetFrequencyRatio",   (PyCFunction)PySDL_MixerTrack_GetFrequencyRatio,   METH_NOARGS  },
    { "SetOutputChannelMap", (PyCFunction)PySDL_MixerTrack_SetOutputChannelMap, METH_O       },
    { "SetStereo",           (PyCFunction)PySDL_MixerTrack_SetStereo,           METH_O       },
    { "Set3DPosition",       (PyCFunction)PySDL_MixerTrack_Set3DPosition,       METH_O       },
    { "Get3DPosition",       (PyCFunction)PySDL_MixerTrack_Get3DPosition,       METH_NOARGS  },
    { "SetGroup",            (PyCFunction)PySDL_MixerTrack_SetGroup,            METH_O       },
    { "SetStoppedCallback",  (PyCFunction)PySDL_MixerTrack_SetStoppedCallback,  METH_O       },
    { "SetRawCallback",      (PyCFunction)PySDL_MixerTrack_SetRawCallback,      METH_O       },
    { "SetCookedCallback",   (PyCFunction)PySDL_MixerTrack_SetCookedCallback,   METH_O       },
    { NULL }
};

PyTypeObject PySDL_MixerTrack_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.MixerTrack",
    .tp_basicsize = sizeof(PySDL_MixerTrack),
    .tp_dealloc   = (destructor)PySDL_MixerTrack_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.MixerTrack(mixer) - MIX_Track (also Mixer.CreateTrack())",
    .tp_methods   = PySDL_MixerTrack_methods,
    .tp_init      = (initproc)PySDL_MixerTrack_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_MixerTrack_Type_init(PySDL_MixerTrack *self, PyObject *args, PyObject *kwds) {
    PyObject *mixer_py;
    static char *kwlist[] = {"mixer", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O!", kwlist, &PySDL_Mixer_Type, &mixer_py)) {
        return -1;
    }
    if(NULL != self->track) {
        PyErr_SetString(pysdl_Error, "MixerTrack is already initialized");
        return -1;
    }
    _Held held;
    if(!_hold((PySDL_Mixer *)mixer_py, &held)) {
        return -1;
    }
    MIX_Track *track = MIX_CreateTrack(held.mixer);
    if(NULL == track) {
        _release(&held);
        _raise();
        return -1;
    }
    if(0 == self->token) {
        self->token = ++_next_token;
    }
    if(0 > PySDL_RegistryAdd(&_tracks, (PyObject *)self)) {
        MIX_DestroyTrack(track);
        _release(&held);
        return -1;
    }
    self->track = track;
    Py_XSETREF(self->owner, (PySDL_Mixer *)Py_NewRef(mixer_py));
    _release(&held);
    return 0;
}

static PyObject * _new_track(PySDL_Mixer *owner) {
    return PyObject_CallOneArg((PyObject *)&PySDL_MixerTrack_Type, (PyObject *)owner);
}

static void _track_destroy(PySDL_MixerTrack *self) {
    // Unlisted first: a callback already waiting for the GIL then finds neither
    // its callables nor this (dying) wrapper.
    _slots_drop(self->token);
    if(NULL != self->track) {
        _Held held;
        if(_hold(self->owner, &held)) {
            if(NULL != self->track) {
                MIX_DestroyTrack(self->track);  // from the mixing thread: deferred by SDL
                self->track = NULL;
            }
            _release(&held);
        } else {
            PyErr_Clear();
        }
    }
    Py_CLEAR(self->input);
}

static void PySDL_MixerTrack_Type_dealloc(PySDL_MixerTrack *self) {
    PySDL_RegistryRemove(&_tracks, (PyObject *)self);
    PyObject *et, *ev, *etb;
    PyErr_Fetch(&et, &ev, &etb);
    _track_destroy(self);
    PyErr_Restore(et, ev, etb);
    Py_CLEAR(self->owner);  // may free the Mixer (deferred if on its mixing thread)
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject * PySDL_MixerTrack_Destroy(PySDL_MixerTrack *self, PyObject *ign) {
    if(_tls_pcm) {
        PyErr_SetString(pysdl_Error, "a track cannot be changed from a raw / cooked mix callback");
        return NULL;
    }
    _track_destroy(self);
    Py_RETURN_NONE;
}

static PyObject * PySDL_MixerTrack_GetMixer(PySDL_MixerTrack *self, PyObject *ign) {
    if(NULL == self->track || NULL == self->owner) {
        return _gone("MixerTrack");
    }
    return Py_NewRef((PyObject *)self->owner);
}

// Run `call` on the held track; set *ok. Shared body of the simple setters.
#define TRACK_BEGIN(mutates)                                            \
    _Held held;                                                         \
    MIX_Track *track = _hold_track(self, &held, (mutates));             \
    if(NULL == track) {                                                 \
        return NULL;                                                    \
    }

#define TRACK_END_BOOL(ok)                                              \
    _release(&held);                                                    \
    if(!(ok)) {                                                         \
        return _raise();                                                \
    }                                                                   \
    Py_RETURN_NONE;

static PyObject * PySDL_MixerTrack_SetAudio(PySDL_MixerTrack *self, PyObject *arg) {
    if(Py_None != arg && NULL == _audio_arg(arg)) {
        return NULL;
    }
    TRACK_BEGIN(1)
    MIX_Audio *audio = (Py_None == arg) ? NULL : _audio_arg(arg);
    if(Py_None != arg && NULL == audio) {
        _release(&held);
        return NULL;
    }
    bool ok = MIX_SetTrackAudio(track, audio);  // refcounts the audio
    if(ok) {
        Py_XSETREF(self->input, (Py_None == arg) ? NULL : Py_NewRef(arg));
    }
    TRACK_END_BOOL(ok)
}

static PyObject * PySDL_MixerTrack_GetAudio(PySDL_MixerTrack *self, PyObject *ign) {
    if(NULL == self->track) {
        return _gone("MixerTrack");
    }
    if(NULL != self->input && PyObject_TypeCheck(self->input, &PySDL_MixerAudio_Type)) {
        return Py_NewRef(self->input);
    }
    Py_RETURN_NONE;
}

// The track reads from the stream while mixing, so the AudioStream wrapper is
// kept alive until the input changes or the track goes away.
static PyObject * PySDL_MixerTrack_SetAudioStream(PySDL_MixerTrack *self, PyObject *arg) {
    if(Py_None != arg) {
        if(!PyObject_TypeCheck(arg, &PySDL_AudioStream_Type)) {
            PyErr_Format(PyExc_TypeError, "expected an SDL3.AudioStream or None, not %.200s", Py_TYPE(arg)->tp_name);
            return NULL;
        }
        if(NULL == ((PySDL_AudioStream *)arg)->stream) {
            PyErr_SetString(pysdl_Error, "AudioStream is not initialized");
            return NULL;
        }
    }
    TRACK_BEGIN(1)
    bool ok = MIX_SetTrackAudioStream(track, (Py_None == arg) ? NULL : ((PySDL_AudioStream *)arg)->stream);
    if(ok) {
        Py_XSETREF(self->input, (Py_None == arg) ? NULL : Py_NewRef(arg));
    }
    TRACK_END_BOOL(ok)
}

static PyObject * PySDL_MixerTrack_GetAudioStream(PySDL_MixerTrack *self, PyObject *ign) {
    if(NULL == self->track) {
        return _gone("MixerTrack");
    }
    if(NULL != self->input && PyObject_TypeCheck(self->input, &PySDL_AudioStream_Type)) {
        return Py_NewRef(self->input);
    }
    Py_RETURN_NONE;
}

// A file to stream from on demand: a path, or the file's bytes (copied into a
// stream the mixer owns, so nothing of Python's is pinned), or None.
static PyObject * _set_io(PySDL_MixerTrack *self, PyObject *src, PyObject *spec_py) {
    SDL_AudioSpec spec;
    if(NULL != spec_py && !_spec_from_py(spec_py, &spec)) {
        return NULL;
    }
    SDL_IOStream *io = NULL;
    if(Py_None != src && NULL == (io = _owned_io(src))) {
        return NULL;
    }
    _Held held;
    MIX_Track *track = _hold_track(self, &held, 1);
    if(NULL == track) {
        if(NULL != io) {
            SDL_CloseIO(io);
        }
        return NULL;
    }
    // SDL_mixer parses the stream here and closes it once done with it (also on failure).
    bool ok = (NULL == spec_py) ? MIX_SetTrackIOStream(track, io, true)
                                : MIX_SetTrackRawIOStream(track, io, &spec, true);
    if(ok) {
        Py_CLEAR(self->input);
    }
    TRACK_END_BOOL(ok)
}

static PyObject * PySDL_MixerTrack_SetIOStream(PySDL_MixerTrack *self, PyObject *arg) {
    return _set_io(self, arg, NULL);
}

static PyObject * PySDL_MixerTrack_SetRawIOStream(PySDL_MixerTrack *self, PyObject *args) {
    PyObject *src, *spec_py;
    if(!PyArg_ParseTuple(args, "OO", &src, &spec_py)) {
        return NULL;
    }
    return _set_io(self, src, spec_py);
}

static PyObject * PySDL_MixerTrack_Tag(PySDL_MixerTrack *self, PyObject *arg) {
    const char *tag = PyUnicode_AsUTF8(arg);
    if(NULL == tag) {
        return NULL;
    }
    TRACK_BEGIN(1)
    bool ok = MIX_TagTrack(track, tag);
    TRACK_END_BOOL(ok)
}

static PyObject * PySDL_MixerTrack_Untag(PySDL_MixerTrack *self, PyObject *arg) {
    const char *tag = PyUnicode_AsUTF8(arg);
    if(NULL == tag) {
        return NULL;
    }
    TRACK_BEGIN(1)
    MIX_UntagTrack(track, tag);
    TRACK_END_BOOL(true)
}

static PyObject * PySDL_MixerTrack_GetTags(PySDL_MixerTrack *self, PyObject *ign) {
    TRACK_BEGIN(0)
    int count = 0;
    char **tags = MIX_GetTrackTags(track, &count);
    _release(&held);
    if(NULL == tags) {
        return _raise();
    }
    PyObject *list = PyList_New(0);
    for(int idx = 0; list && idx < count; ++idx) {
        PyObject *str = PyUnicode_FromString(tags[idx]);
        if(NULL == str || 0 > PyList_Append(list, str)) {
            Py_CLEAR(list);
        }
        Py_XDECREF(str);
    }
    SDL_free(tags);
    return list;
}

static PyObject * PySDL_MixerTrack_SetPlaybackPosition(PySDL_MixerTrack *self, PyObject *arg) {
    long long frames = PyLong_AsLongLong(arg);
    if(-1 == frames && PyErr_Occurred()) {
        return NULL;
    }
    TRACK_BEGIN(1)
    bool ok = MIX_SetTrackPlaybackPosition(track, frames);
    TRACK_END_BOOL(ok)
}

#define TRACK_GET_INT(name, call)                                       \
    static PyObject * name(PySDL_MixerTrack *self, PyObject *ign) {     \
        TRACK_BEGIN(0)                                                  \
        long long value = (long long)call(track);                       \
        _release(&held);                                                \
        return PyLong_FromLongLong(value);                              \
    }

TRACK_GET_INT(PySDL_MixerTrack_GetPlaybackPosition, MIX_GetTrackPlaybackPosition)
TRACK_GET_INT(PySDL_MixerTrack_GetFadeFrames,       MIX_GetTrackFadeFrames)
TRACK_GET_INT(PySDL_MixerTrack_GetLoops,            MIX_GetTrackLoops)
TRACK_GET_INT(PySDL_MixerTrack_GetRemaining,        MIX_GetTrackRemaining)

static PyObject * PySDL_MixerTrack_SetLoops(PySDL_MixerTrack *self, PyObject *arg) {
    int loops;
    if(!_as_int(arg, &loops)) {
        return NULL;
    }
    TRACK_BEGIN(1)
    bool ok = MIX_SetTrackLoops(track, loops);
    TRACK_END_BOOL(ok)
}

#define TRACK_CONVERT(name, call)                                       \
    static PyObject * name(PySDL_MixerTrack *self, PyObject *arg) {     \
        long long value = PyLong_AsLongLong(arg);                       \
        if(-1 == value && PyErr_Occurred()) {                           \
            return NULL;                                                \
        }                                                               \
        TRACK_BEGIN(0)                                                  \
        long long result = (long long)call(track, value);               \
        _release(&held);                                                \
        return PyLong_FromLongLong(result);                             \
    }

TRACK_CONVERT(PySDL_MixerTrack_MSToFrames, MIX_TrackMSToFrames)
TRACK_CONVERT(PySDL_MixerTrack_FramesToMS, MIX_TrackFramesToMS)

// Play(**options): loops, max_frame / max_ms, start_frame / start_ms,
// loop_start_frame / loop_start_ms, fade_in_frames / fade_in_ms,
// fade_in_start_gain, append_silence_frames / append_silence_ms,
// halt_when_exhausted (and start_order with SDL_mixer >= 3.2.2).
static PyObject * PySDL_MixerTrack_Play(PySDL_MixerTrack *self, PyObject *args, PyObject *kwds) {
    if(0 < PyTuple_GET_SIZE(args)) {
        PyErr_SetString(PyExc_TypeError, "Play() takes keyword options only");
        return NULL;
    }
    SDL_PropertiesID props = _play_props(kwds);
    if((SDL_PropertiesID)-1 == props) {
        return NULL;
    }
    _Held held;
    MIX_Track *track = _hold_track(self, &held, 1);
    bool ok = false;
    if(NULL != track) {
        ok = MIX_PlayTrack(track, props);
        if(!ok) {
            PyErr_SetString(pysdl_Error, SDL_GetError());
        }
        _release(&held);
    }
    if(0 != props) {
        SDL_DestroyProperties(props);
    }
    if(!ok) {
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_MixerTrack_Stop(PySDL_MixerTrack *self, PyObject *args, PyObject *kwds) {
    long long fade_out_frames = 0;
    PyObject *fade_out_ms = Py_None;
    static char *kwlist[] = {"fade_out_frames", "fade_out_ms", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "|L$O", kwlist, &fade_out_frames, &fade_out_ms)) {
        return NULL;
    }
    long long ms = 0;
    if(Py_None != fade_out_ms && (-1 == (ms = PyLong_AsLongLong(fade_out_ms)) && PyErr_Occurred())) {
        return NULL;
    }
    TRACK_BEGIN(1)
    if(Py_None != fade_out_ms) {
        fade_out_frames = MIX_TrackMSToFrames(track, ms);
    }
    bool ok = MIX_StopTrack(track, fade_out_frames);
    TRACK_END_BOOL(ok)
}

#define TRACK_ACTION(name, call)                                        \
    static PyObject * name(PySDL_MixerTrack *self, PyObject *ign) {     \
        TRACK_BEGIN(1)                                                  \
        bool ok = call(track);                                          \
        TRACK_END_BOOL(ok)                                              \
    }

TRACK_ACTION(PySDL_MixerTrack_Pause,  MIX_PauseTrack)
TRACK_ACTION(PySDL_MixerTrack_Resume, MIX_ResumeTrack)

#define TRACK_QUERY(name, call)                                         \
    static PyObject * name(PySDL_MixerTrack *self, PyObject *ign) {     \
        TRACK_BEGIN(0)                                                  \
        bool value = call(track);                                       \
        _release(&held);                                                \
        return PyBool_FromLong(value);                                  \
    }

TRACK_QUERY(PySDL_MixerTrack_Playing, MIX_TrackPlaying)
TRACK_QUERY(PySDL_MixerTrack_Paused,  MIX_TrackPaused)

static PyObject * PySDL_MixerTrack_SetGain(PySDL_MixerTrack *self, PyObject *arg) {
    double gain = PyFloat_AsDouble(arg);
    if(-1.0 == gain && PyErr_Occurred()) {
        return NULL;
    }
    TRACK_BEGIN(1)
    bool ok = MIX_SetTrackGain(track, (float)gain);
    TRACK_END_BOOL(ok)
}

static PyObject * PySDL_MixerTrack_GetGain(PySDL_MixerTrack *self, PyObject *ign) {
    TRACK_BEGIN(0)
    float gain = MIX_GetTrackGain(track);
    _release(&held);
    return PyFloat_FromDouble(gain);
}

static PyObject * PySDL_MixerTrack_SetFrequencyRatio(PySDL_MixerTrack *self, PyObject *arg) {
    double ratio = PyFloat_AsDouble(arg);
    if(-1.0 == ratio && PyErr_Occurred()) {
        return NULL;
    }
    TRACK_BEGIN(1)
    bool ok = MIX_SetTrackFrequencyRatio(track, (float)ratio);
    TRACK_END_BOOL(ok)
}

static PyObject * PySDL_MixerTrack_GetFrequencyRatio(PySDL_MixerTrack *self, PyObject *ign) {
    TRACK_BEGIN(0)
    float ratio = MIX_GetTrackFrequencyRatio(track);
    _release(&held);
    return PyFloat_FromDouble(ratio);
}

// A sequence of output channels (-1 mutes one), or None to stop remapping.
static PyObject * PySDL_MixerTrack_SetOutputChannelMap(PySDL_MixerTrack *self, PyObject *arg) {
    int *chmap = NULL;
    int count = 0;
    if(Py_None != arg) {
        PyObject *fast = PySequence_Fast(arg, "the channel map must be a sequence of ints or None");
        if(NULL == fast) {
            return NULL;
        }
        Py_ssize_t len = PySequence_Fast_GET_SIZE(fast);
        if(0 == len || len > 256) {
            Py_DECREF(fast);
            PyErr_SetString(PyExc_ValueError, "the channel map needs 1 to 256 entries");
            return NULL;
        }
        chmap = PyMem_New(int, len);
        if(NULL == chmap) {
            Py_DECREF(fast);
            return PyErr_NoMemory();
        }
        for(Py_ssize_t idx = 0; idx < len; ++idx) {
            if(!_as_int(PySequence_Fast_GET_ITEM(fast, idx), &chmap[idx])) {
                Py_DECREF(fast);
                PyMem_Free(chmap);
                return NULL;
            }
        }
        Py_DECREF(fast);
        count = (int)len;
    }
    _Held held;
    MIX_Track *track = _hold_track(self, &held, 1);
    if(NULL == track) {
        PyMem_Free(chmap);
        return NULL;
    }
    bool ok;
    if(NULL != chmap) {
        ok = MIX_SetTrackOutputChannelMap(track, chmap, count);  // copied
        PyMem_Free(chmap);
    } else {
        // SDL wants the stream's channel count even to reset the map (SDL_mixer's
        // docs say NULL is enough); the output has the mixer's channels, or 2 /
        // 1 in SetStereo / Set3DPosition mode. A wrong count changes nothing.
        SDL_AudioSpec spec;
        int candidates[3] = { MIX_GetMixerFormat(held.mixer, &spec) ? spec.channels : 2, 2, 1 };
        ok = false;
        for(int idx = 0; idx < 3 && !ok; ++idx) {
            ok = MIX_SetTrackOutputChannelMap(track, NULL, candidates[idx]);
        }
    }
    TRACK_END_BOOL(ok)
}

static PyObject * PySDL_MixerTrack_SetStereo(PySDL_MixerTrack *self, PyObject *arg) {
    MIX_StereoGains gains;
    if(Py_None != arg && !PyArg_ParseTuple(arg, "ff;stereo gains are (left, right)", &gains.left, &gains.right)) {
        return NULL;
    }
    TRACK_BEGIN(1)
    bool ok = MIX_SetTrackStereo(track, (Py_None == arg) ? NULL : &gains);
    TRACK_END_BOOL(ok)
}

static PyObject * PySDL_MixerTrack_Set3DPosition(PySDL_MixerTrack *self, PyObject *arg) {
    MIX_Point3D position;
    if(Py_None != arg && !PyArg_ParseTuple(arg, "fff;a 3D position is (x, y, z)", &position.x, &position.y, &position.z)) {
        return NULL;
    }
    TRACK_BEGIN(1)
    bool ok = MIX_SetTrack3DPosition(track, (Py_None == arg) ? NULL : &position);
    TRACK_END_BOOL(ok)
}

static PyObject * PySDL_MixerTrack_Get3DPosition(PySDL_MixerTrack *self, PyObject *ign) {
    TRACK_BEGIN(0)
    MIX_Point3D position;
    bool ok = MIX_GetTrack3DPosition(track, &position);
    _release(&held);
    if(!ok) {
        return _raise();
    }
    return Py_BuildValue("(fff)", position.x, position.y, position.z);
}

static PyObject * PySDL_MixerTrack_SetGroup(PySDL_MixerTrack *self, PyObject *arg) {
    if(Py_None != arg && !PyObject_TypeCheck(arg, &PySDL_MixerGroup_Type)) {
        PyErr_Format(PyExc_TypeError, "expected an SDL3.MixerGroup or None, not %.200s", Py_TYPE(arg)->tp_name);
        return NULL;
    }
    TRACK_BEGIN(1)
    MIX_Group *group = NULL;
    if(Py_None != arg) {
        PySDL_MixerGroup *wrapper = (PySDL_MixerGroup *)arg;
        if(NULL == wrapper->group || wrapper->owner != self->owner) {
            _release(&held);
            if(NULL == wrapper->group) {
                return _gone("MixerGroup");
            }
            PyErr_SetString(pysdl_Error, "the track and the group belong to different mixers");
            return NULL;
        }
        group = wrapper->group;
    }
    bool ok = MIX_SetTrackGroup(track, group);
    TRACK_END_BOOL(ok)
}

// fn(track) when the track stops (ran out, or Stop()); not when paused or
// destroyed. It may restart the track or give it new input.
static PyObject * PySDL_MixerTrack_SetStoppedCallback(PySDL_MixerTrack *self, PyObject *fn) {
    TRACK_BEGIN(1)
    SET_CALLBACK(MIX_SetTrackStoppedCallback, track, _track_stopped, self->token, SLOT_STOPPED, SLOT_COUNT, fn);
    _release(&held);
    Py_RETURN_NONE;
fail:
    _release(&held);
    return NULL;
}

// fn(track, spec, pcm) with float32 samples as bytes (spec is always F32);
// return same-sized bytes to replace them, or None. Raw runs on the decoded
// input, cooked after the track's gain / panning / channel map.
static PyObject * PySDL_MixerTrack_SetRawCallback(PySDL_MixerTrack *self, PyObject *fn) {
    TRACK_BEGIN(1)
    SET_CALLBACK(MIX_SetTrackRawCallback, track, _track_raw, self->token, SLOT_RAW, SLOT_COUNT, fn);
    _release(&held);
    Py_RETURN_NONE;
fail:
    _release(&held);
    return NULL;
}

static PyObject * PySDL_MixerTrack_SetCookedCallback(PySDL_MixerTrack *self, PyObject *fn) {
    TRACK_BEGIN(1)
    SET_CALLBACK(MIX_SetTrackCookedCallback, track, _track_cooked, self->token, SLOT_COOKED, SLOT_COUNT, fn);
    _release(&held);
    Py_RETURN_NONE;
fail:
    _release(&held);
    return NULL;
}

//=========================================================
// SDL3.MixerGroup
//=========================================================

static int        PySDL_MixerGroup_Type_init    (PySDL_MixerGroup*, PyObject*, PyObject*);
static void       PySDL_MixerGroup_Type_dealloc (PySDL_MixerGroup*);

static PyObject * PySDL_MixerGroup_Destroy            (PySDL_MixerGroup*, PyObject*);
static PyObject * PySDL_MixerGroup_GetMixer           (PySDL_MixerGroup*, PyObject*);
static PyObject * PySDL_MixerGroup_SetPostMixCallback (PySDL_MixerGroup*, PyObject*);

static PyMethodDef PySDL_MixerGroup_methods[] = {
    { "Destroy",            (PyCFunction)PySDL_MixerGroup_Destroy,            METH_NOARGS },
    { "GetMixer",           (PyCFunction)PySDL_MixerGroup_GetMixer,           METH_NOARGS },
    { "SetPostMixCallback", (PyCFunction)PySDL_MixerGroup_SetPostMixCallback, METH_O      },
    { NULL }
};

PyTypeObject PySDL_MixerGroup_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.MixerGroup",
    .tp_basicsize = sizeof(PySDL_MixerGroup),
    .tp_dealloc   = (destructor)PySDL_MixerGroup_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.MixerGroup(mixer) - MIX_Group (also Mixer.CreateGroup())",
    .tp_methods   = PySDL_MixerGroup_methods,
    .tp_init      = (initproc)PySDL_MixerGroup_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_MixerGroup_Type_init(PySDL_MixerGroup *self, PyObject *args, PyObject *kwds) {
    PyObject *mixer_py;
    static char *kwlist[] = {"mixer", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O!", kwlist, &PySDL_Mixer_Type, &mixer_py)) {
        return -1;
    }
    if(NULL != self->group) {
        PyErr_SetString(pysdl_Error, "MixerGroup is already initialized");
        return -1;
    }
    _Held held;
    if(!_hold((PySDL_Mixer *)mixer_py, &held)) {
        return -1;
    }
    MIX_Group *group = MIX_CreateGroup(held.mixer);
    if(NULL == group) {
        _release(&held);
        _raise();
        return -1;
    }
    if(0 == self->token) {
        self->token = ++_next_token;
    }
    if(0 > PySDL_RegistryAdd(&_groups, (PyObject *)self)) {
        MIX_DestroyGroup(group);
        _release(&held);
        return -1;
    }
    self->group = group;
    Py_XSETREF(self->owner, (PySDL_Mixer *)Py_NewRef(mixer_py));
    _release(&held);
    return 0;
}

static PyObject * _new_group(PySDL_Mixer *owner) {
    return PyObject_CallOneArg((PyObject *)&PySDL_MixerGroup_Type, (PyObject *)owner);
}

static void _group_destroy(PySDL_MixerGroup *self) {
    _slots_drop(self->token);
    if(NULL != self->group) {
        _Held held;
        if(_hold(self->owner, &held)) {
            if(NULL != self->group) {
                MIX_DestroyGroup(self->group);  // its tracks go back to the default group
                self->group = NULL;
            }
            _release(&held);
        } else {
            PyErr_Clear();
        }
    }
}

static void PySDL_MixerGroup_Type_dealloc(PySDL_MixerGroup *self) {
    PySDL_RegistryRemove(&_groups, (PyObject *)self);
    PyObject *et, *ev, *etb;
    PyErr_Fetch(&et, &ev, &etb);
    _group_destroy(self);
    PyErr_Restore(et, ev, etb);
    Py_CLEAR(self->owner);
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject * PySDL_MixerGroup_Destroy(PySDL_MixerGroup *self, PyObject *ign) {
    _group_destroy(self);
    Py_RETURN_NONE;
}

static PyObject * PySDL_MixerGroup_GetMixer(PySDL_MixerGroup *self, PyObject *ign) {
    if(NULL == self->group || NULL == self->owner) {
        return _gone("MixerGroup");
    }
    return Py_NewRef((PyObject *)self->owner);
}

// fn(group, spec, pcm) once the group's tracks are mixed together (float32
// bytes; return same-sized bytes to replace them, or None).
static PyObject * PySDL_MixerGroup_SetPostMixCallback(PySDL_MixerGroup *self, PyObject *fn) {
    _Held held;
    MIX_Group *group = _hold_group(self, &held);
    if(NULL == group) {
        return NULL;
    }
    SET_CALLBACK(MIX_SetGroupPostMixCallback, group, _group_post_mix, self->token, 0, 1, fn);
    _release(&held);
    Py_RETURN_NONE;
fail:
    _release(&held);
    return NULL;
}

//=========================================================
// SDL3.MixerDecoder
//=========================================================

static int        PySDL_MixerDecoder_Type_init    (PySDL_MixerDecoder*, PyObject*, PyObject*);
static void       PySDL_MixerDecoder_Type_dealloc (PySDL_MixerDecoder*);

static PyObject * PySDL_MixerDecoder_Destroy   (PySDL_MixerDecoder*, PyObject*);
static PyObject * PySDL_MixerDecoder_GetFormat (PySDL_MixerDecoder*, PyObject*);
static PyObject * PySDL_MixerDecoder_Decode    (PySDL_MixerDecoder*, PyObject*, PyObject*);

static PyMethodDef PySDL_MixerDecoder_methods[] = {
    { "Destroy",   (PyCFunction)PySDL_MixerDecoder_Destroy,   METH_NOARGS },
    { "GetFormat", (PyCFunction)PySDL_MixerDecoder_GetFormat, METH_NOARGS },
    { "Decode",    (PyCFunction)PySDL_MixerDecoder_Decode,    METH_VARARGS | METH_KEYWORDS },
    { NULL }
};

PyTypeObject PySDL_MixerDecoder_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "SDL3.MixerDecoder",
    .tp_basicsize = sizeof(PySDL_MixerDecoder),
    .tp_dealloc   = (destructor)PySDL_MixerDecoder_Type_dealloc,
    .tp_flags     = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc       = "SDL3.MixerDecoder(src, *, decoder=None) - MIX_AudioDecoder: decode a file "
                    "(path or bytes) to PCM without playing it",
    .tp_methods   = PySDL_MixerDecoder_methods,
    .tp_init      = (initproc)PySDL_MixerDecoder_Type_init,
    .tp_new       = PyType_GenericNew
};

static int PySDL_MixerDecoder_Type_init(PySDL_MixerDecoder *self, PyObject *args, PyObject *kwds) {
    PyObject *src;
    const char *decoder = NULL;
    static char *kwlist[] = {"src", "decoder", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "O|$z", kwlist, &src, &decoder)) {
        return -1;
    }
    if(NULL != self->decoder) {
        PyErr_SetString(pysdl_Error, "MixerDecoder is already initialized");
        return -1;
    }
    if(!_need_init() || 0 > PySDL_RegistryAdd(&_decoders, (PyObject *)self)) {
        return -1;
    }
    SDL_IOStream *io = _owned_io(src);  // read on demand by Decode()
    if(NULL == io) {
        return -1;
    }
    SDL_PropertiesID props = 0;
    if(NULL != decoder) {
        props = SDL_CreateProperties();
        if(0 == props || !SDL_SetStringProperty(props, MIX_PROP_AUDIO_DECODER_STRING, decoder)) {
            SDL_DestroyProperties(props);
            SDL_CloseIO(io);
            _raise();
            return -1;
        }
    }
    MIX_AudioDecoder *made;
    Py_BEGIN_ALLOW_THREADS
        made = MIX_CreateAudioDecoder_IO(io, true, props);  // closes `io` on failure too
    Py_END_ALLOW_THREADS
    if(0 != props) {
        SDL_DestroyProperties(props);
    }
    if(NULL == made) {
        _raise();
        return -1;
    }
    self->decoder = made;
    return 0;
}

static int _decoder_ready(PySDL_MixerDecoder *self) {
    if(NULL == self->decoder) {
        _gone("MixerDecoder");
        return 0;
    }
    if(self->busy) {
        PyErr_SetString(pysdl_Error, "MixerDecoder is decoding in another thread");
        return 0;
    }
    return 1;
}

static void PySDL_MixerDecoder_Type_dealloc(PySDL_MixerDecoder *self) {
    PySDL_RegistryRemove(&_decoders, (PyObject *)self);
    if(NULL != self->decoder) {
        MIX_DestroyAudioDecoder(self->decoder);  // busy is 0: Decode holds a reference
        self->decoder = NULL;
    }
    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject * PySDL_MixerDecoder_Destroy(PySDL_MixerDecoder *self, PyObject *ign) {
    if(NULL != self->decoder) {
        if(!_decoder_ready(self)) {
            return NULL;
        }
        MIX_DestroyAudioDecoder(self->decoder);
        self->decoder = NULL;
    }
    Py_RETURN_NONE;
}

static PyObject * PySDL_MixerDecoder_GetFormat(PySDL_MixerDecoder *self, PyObject *ign) {
    if(!_decoder_ready(self)) {
        return NULL;
    }
    SDL_AudioSpec spec;
    if(!MIX_GetAudioDecoderFormat(self->decoder, &spec)) {
        return _raise();
    }
    return _spec_to_py(&spec);
}

// Decode(nbytes, spec=None) -> up to nbytes of PCM in `spec` (default: the
// file's own format); b'' once the whole file is decoded.
static PyObject * PySDL_MixerDecoder_Decode(PySDL_MixerDecoder *self, PyObject *args, PyObject *kwds) {
    Py_ssize_t nbytes;
    PyObject *spec_py = Py_None;
    static char *kwlist[] = {"nbytes", "spec", NULL};
    if(!PyArg_ParseTupleAndKeywords(args, kwds, "n|O", kwlist, &nbytes, &spec_py)) {
        return NULL;
    }
    if(0 > nbytes || nbytes > INT_MAX) {
        PyErr_SetString(PyExc_ValueError, "nbytes must be between 0 and INT_MAX");
        return NULL;
    }
    if(!_decoder_ready(self)) {
        return NULL;
    }
    SDL_AudioSpec spec;
    if(Py_None == spec_py ? !MIX_GetAudioDecoderFormat(self->decoder, &spec) : !_spec_from_py(spec_py, &spec)) {
        return PyErr_Occurred() ? NULL : _raise();
    }
    PyObject *data = PyBytes_FromStringAndSize(NULL, nbytes);
    if(NULL == data) {
        return NULL;
    }
    MIX_AudioDecoder *decoder = self->decoder;
    self->busy = 1;
    Py_INCREF(self);
    int got;
    Py_BEGIN_ALLOW_THREADS
        got = MIX_DecodeAudio(decoder, PyBytes_AS_STRING(data), (int)nbytes, &spec);
    Py_END_ALLOW_THREADS
    self->busy = 0;
    Py_DECREF(self);
    if(0 > got) {
        Py_DECREF(data);
        return _raise();
    }
    if(got < nbytes && 0 > _PyBytes_Resize(&data, got)) {
        return NULL;
    }
    return data;
}

//=========================================================
// registration
//=========================================================

int PySDL_Mixer_Register(PyObject *module) {
    static const struct { PyTypeObject *type; const char *name; } types[] = {
        { &PySDL_Mixer_Type,        "Mixer"        },
        { &PySDL_MixerAudio_Type,   "MixerAudio"   },
        { &PySDL_MixerTrack_Type,   "MixerTrack"   },
        { &PySDL_MixerGroup_Type,   "MixerGroup"   },
        { &PySDL_MixerDecoder_Type, "MixerDecoder" },
    };
    for(size_t idx = 0; idx < SDL_arraysize(types); ++idx) {
        if(0 > PyType_Ready(types[idx].type)
            || 0 > PyModule_AddObjectRef(module, types[idx].name, (PyObject *)types[idx].type)) {
            return -1;
        }
    }
    if(0 > PyModule_AddFunctions(module, pysdl_mixer_methods)
        || 0 > PyModule_AddIntConstant(module, "MIX_DURATION_UNKNOWN",  MIX_DURATION_UNKNOWN)
        || 0 > PyModule_AddIntConstant(module, "MIX_DURATION_INFINITE", MIX_DURATION_INFINITE)
        || 0 > PyModule_AddIntConstant(module, "SDL_MIXER_VERSION",     SDL_MIXER_VERSION)) {
        return -1;
    }
    return 0;
}

#endif // PYSDL_HAVE_MIXER
