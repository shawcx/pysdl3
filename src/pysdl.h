#ifndef __PYSDL_H__
#define __PYSDL_H__

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <structmember.h>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_vulkan.h>
#ifdef PYSDL_HAVE_TTF
    #include <SDL3_ttf/SDL_ttf.h>  // optional: setup.py defines PYSDL_HAVE_TTF when found
#endif

#define  DOC_MOD  "Python wrapper for SDL."

#if PY_VERSION_HEX < 0x030D0000
    #define Py_IsFinalizing _Py_IsFinalizing
#endif

extern PyObject *pysdl_Error;

//=========================================================
// Types
//=========================================================

typedef struct {
    PyObject_HEAD
    SDL_Window *window;
    SDL_GLContext glContext;
    int shouldFree;  // 0 for a borrowed window (GetKeyboardFocus, GetMouseFocus, ...)
} PySDL_Window;
extern PyTypeObject PySDL_Window_Type;

typedef struct {
    PyObject_HEAD
    SDL_Renderer *renderer;  // always owned; NULL once SDL destroyed it (see PySDL_InvalidateRenderers)
    PyObject *target;        // Texture currently set as render target, or NULL
    PyObject *surface;       // Surface a software renderer draws to (kept alive), or NULL
} PySDL_Renderer;
extern PyTypeObject PySDL_Renderer_Type;

typedef struct {
    PyObject_HEAD
    SDL_Surface *surface;
    int shouldFree;
    Py_buffer pixels;  // backing buffer for a *...From surface; pixels.obj == NULL otherwise
    SDL_WindowID window_id;  // non-zero: this is that window's surface (tracked in pysdl_Window.c)
} PySDL_Surface;
extern PyTypeObject PySDL_Surface_Type;

typedef struct {
    PyObject_HEAD
    SDL_Texture *texture;
    PyObject *locked;        // Surface from LockToSurface (emptied on Unlock), or NULL
    SDL_Renderer *renderer;  // the renderer that owns `texture` (see PySDL_TextureTrack)
} PySDL_Texture;
extern PyTypeObject PySDL_Texture_Type;

typedef struct {
    PyObject_HEAD
    SDL_PixelFormat format;                 // the enum; SDL_PIXELFORMAT_UNKNOWN if uninitialised
    const SDL_PixelFormatDetails *details;  // cached by SDL, never freed
    PyObject *palette;                      // borrowed SDL3.Palette wrapper (SetPalette), or NULL
} PySDL_PixelFormat;
extern PyTypeObject PySDL_PixelFormat_Type;

typedef struct {
    PyObject_HEAD
    SDL_Palette *palette;
} PySDL_Palette;
extern PyTypeObject PySDL_Palette_Type;

typedef struct {
    PyObject_HEAD
    SDL_AudioDeviceID deviceId;
    SDL_AudioStream *stream;  // device-bound stream (SDL_OpenAudioDeviceStream), or NULL
    PyObject *pycallback;     // (callback, userdata) tuple, or NULL
} PySDL_Audio;
extern PyTypeObject PySDL_Audio_Type;

typedef struct {
    PyObject_HEAD
    SDL_Cursor *cursor;
    int shouldFree;  // 0 for a borrowed cursor (GetCursor, GetDefaultCursor)
} PySDL_Cursor;
extern PyTypeObject PySDL_Cursor_Type;

typedef struct {
    PyObject_HEAD
    SDL_Joystick *joystick;  // always owned: lookups re-open by ID (SDL refcounts)
} PySDL_Joystick;
extern PyTypeObject PySDL_Joystick_Type;

typedef struct {
    PyObject_HEAD
    SDL_Gamepad *gamepad;
} PySDL_Gamepad;
extern PyTypeObject PySDL_Gamepad_Type;

// SDL runs virtual-joystick callbacks - and so Python - holding its joystick
// lock, so a thread that waits for that lock while holding the GIL can
// deadlock against one. PySDL_JoystickLock() takes it with the GIL released
// (a no-op while the joystick subsystem is down) and returns whether it did,
// for PySDL_JoystickUnlock(). Every Python entry point of pysdl_Joystick.c
// and pysdl_Gamepad.c runs inside it: PYSDL_JOYSTICK_LOCKED(name, selftype)
// (METH_NOARGS / METH_O / METH_VARARGS) and PYSDL_JOYSTICK_LOCKED_KW (with
// keywords) declare `name##_impl` and define `name` as that call under the
// lock. The body then runs holding both, so strings SDL returns are copied
// before another thread can close the device.
int  PySDL_JoystickLock(void);
void PySDL_JoystickUnlock(int locked);

#define PYSDL_JOYSTICK_LOCKED(name, selftype)                                   \
    static PyObject * name##_impl(selftype, PyObject *);                       \
    static PyObject * name(selftype self, PyObject *arg) {                     \
        int locked = PySDL_JoystickLock();                                     \
        PyObject *result = name##_impl(self, arg);                             \
        PySDL_JoystickUnlock(locked);                                          \
        return result;                                                         \
    }

#define PYSDL_JOYSTICK_LOCKED_KW(name, selftype)                                \
    static PyObject * name##_impl(selftype, PyObject *, PyObject *);           \
    static PyObject * name(selftype self, PyObject *args, PyObject *kwds) {    \
        int locked = PySDL_JoystickLock();                                     \
        PyObject *result = name##_impl(self, args, kwds);                      \
        PySDL_JoystickUnlock(locked);                                          \
        return result;                                                         \
    }

// Joystick GUIDs cross into Python as 32-char hex strings (pysdl_Joystick.c).
// PyToGUID also accepts the raw 16 bytes; returns 0 with an exception set.
PyObject * GUIDToPy(SDL_GUID guid);
int        PyToGUID(PyObject *obj, SDL_GUID *guid);

typedef struct {
    PyObject_HEAD
    SDL_TimerID id;
    unsigned long long token;  // key of the callable in pysdl_Timer.c's _timers (0: none)
} PySDL_Timer;
extern PyTypeObject PySDL_Timer_Type;

typedef struct {
    PyObject_HEAD
    SDL_Haptic *haptic;
} PySDL_Haptic;
extern PyTypeObject PySDL_Haptic_Type;

typedef struct {
    PyObject_HEAD
    SDL_Sensor *sensor;
} PySDL_Sensor;
extern PyTypeObject PySDL_Sensor_Type;

#ifdef PYSDL_HAVE_TTF
typedef struct {
    PyObject_HEAD
    TTF_Font *font;
    Py_buffer data;    // pins the font file's bytes (SDL_ttf reads them lazily); data.obj == NULL for a path
    unsigned session;  // TTF init session the font was opened in (see pysdl_Font.c)
} PySDL_Font;
extern PyTypeObject PySDL_Font_Type;
#endif

typedef struct {
    PyObject_HEAD
    SDL_AudioStream *stream;
} PySDL_AudioStream;
extern PyTypeObject PySDL_AudioStream_Type;

// generated constant table (tools/gen_constants.py --generate)
void _constants(PyObject *module);

// Extra module-function tables registered from their own files.
extern PyMethodDef pysdl_events_methods[];          // pysdl_Events.c  (event queue)
extern PyMethodDef pysdl_input_methods[];           // pysdl_Input.c   (keyboard / mouse / touch / text)
extern PyMethodDef pysdl_cursor_methods[];          // pysdl_Cursor.c
extern PyMethodDef pysdl_joystick_methods[];        // pysdl_Joystick.c
extern PyMethodDef pysdl_gamepad_methods[];         // pysdl_Gamepad.c
extern PyMethodDef pysdl_haptic_methods[];          // pysdl_Haptic.c
extern PyMethodDef pysdl_sensor_methods[];          // pysdl_Sensor.c
extern PyMethodDef pysdl_video_methods[];           // pysdl_Video.c   (display / messagebox / hints / vulkan / metal)
extern PyMethodDef pysdl_audio_methods[];           // pysdl_Audio.c   (drivers / LoadWAV / mixing)
extern PyMethodDef pysdl_rect_methods[];            // pysdl_Rect.c    (rect / point math)
extern PyMethodDef pysdl_image_methods[];           // pysdl_Image.c   (SDL_image loaders / probes / animation)
#ifdef PYSDL_HAVE_TTF
extern PyMethodDef pysdl_ttf_methods[];             // pysdl_Font.c    (SDL_ttf init / versions / tags)
int PySDL_TTF_Constants(PyObject *module);          // pysdl_Font.c    (TTF_STYLE_* / TTF_HINTING_* / ...)
#endif

// SDL_image texture loading shared with Renderer.LoadTexture (pysdl_Image.c):
// `src` is a path or the file's bytes, `type` names a magic-less format
// ("TGA") or is NULL. NULL with an exception set on failure.
SDL_Texture * PySDL_IMG_LoadTexture(SDL_Renderer *renderer, PyObject *src, const char *type);

//=========================================================
// Helpers (pysdl_util.c)
//=========================================================

// Allocate a wrapper instance of `type` (defined in pysdl.c); sets a TypeError
// and returns NULL on failure. Replaces the repeated PyObject_CallObject boilerplate.
PyObject * PySDL_New(PyTypeObject *type);

// Enter/leave the interpreter from an SDL-owned thread (audio, timer, ...).
// PySDL_ThreadEnter returns 0 without acquiring the GIL if Python is shutting
// down, in which case the caller must not touch any Python object.
int  PySDL_ThreadEnter(PyGILState_STATE *state);
void PySDL_ThreadLeave(PyGILState_STATE state);

// Convert a Python tuple/list to an SDL struct. Return 1 on success, or 0 with
// an exception set (usable directly as an "O&" converter).
int PyToRect(PyObject *src, SDL_Rect *dst);     // 2 items -> w/h = -1, or 4 items
int PyToPoint(PyObject *src, SDL_Point *dst);   // 2 items
int PyToColor(PyObject *src, SDL_Color *dst);   // 3 items -> a = 255, or 4 items
int PyToFRect(PyObject *src, SDL_FRect *dst);   // 2 items -> w/h = -1, or 4 items
int PyToFPoint(PyObject *src, SDL_FPoint *dst); // 2 items

// A pixel value: an int is taken verbatim, a 3/4-item sequence is mapped
// through `format` (+ `palette`, which may be NULL).
int PyToPixel(PyObject *src, const SDL_PixelFormatDetails *format,
              const SDL_Palette *palette, Uint32 *out);

PyObject * RectToPy(const SDL_Rect *rect);      // -> (x, y, w, h)
PyObject * PointToPy(const SDL_Point *point);   // -> (x, y)

// Wrap a window SDL still owns as a non-freeing SDL3.Window, or None for NULL.
PyObject * PySDL_WrapWindow(SDL_Window *window);

// The surface of a Surface argument: 0 with an exception set if `obj` is not
// an SDL3.Surface or wraps none (an uninitialised SDL3.Surface()), where SDL
// would read NULL as "no surface".
int PySDL_SurfaceArg(PyObject *obj, SDL_Surface **out);

// File sources: anything that reads a file takes a path (str / os.PathLike)
// or the file's contents (any bytes-like).
//
// 1 and a new str reference in *path if `src` is a path, 0 if it is not (so
// it is the contents), -1 with an exception set (e.g. a bytes os.PathLike).
int PySDL_PathFromObject(PyObject *src, PyObject **path);

// A readable SDL_IOStream over `src`: a path is opened as a file (GIL
// released for the open), contents are read in place and pinned by `view`.
// Call PyBuffer_Release(view) once SDL has closed the stream (a no-op for a
// file, where view->obj stays NULL). NULL with an exception set on failure.
SDL_IOStream * PySDL_IOFromObject(PyObject *src, Py_buffer *view);

// Save* destinations: `dest` is a path (str / os.PathLike) or None for "return
// the bytes". PySDL_SaveOpen sets *path to the path as a new str reference
// (NULL for None) and returns a stream writing that file, or a growable
// in-memory stream; NULL with an exception set on failure. PySDL_SaveClose
// closes it after SDL wrote to it (`ok` = the writer's result) and returns
// None (file) or the written bytes (memory), or raises SDL's error; it
// releases `path` either way.
SDL_IOStream * PySDL_SaveOpen(PyObject *dest, PyObject **path);
PyObject     * PySDL_SaveClose(SDL_IOStream *io, PyObject *path, bool ok);

// A set of *borrowed* wrapper pointers: each wrapper adds itself when it takes
// an SDL object and removes itself in tp_dealloc, so the set never holds a
// dead object. Used to invalidate wrappers when SDL frees what they point at.
typedef struct {
    PyObject **items;
    Py_ssize_t len, cap;
} PySDL_Registry;
int  PySDL_RegistryAdd(PySDL_Registry *reg, PyObject *obj);  // no-op if present; -1 on OOM
void PySDL_RegistryRemove(PySDL_Registry *reg, PyObject *obj);

// Point a Surface wrapper whose SDL surface was (or is about to be) freed by
// SDL at a fresh empty 0x0 surface of its own, without freeing the old one, so
// later use is harmless (pysdl_Surface.c).
void PySDL_SurfaceDetach(PySDL_Surface *surface);

// Window / window-surface tracking (pysdl_Window.c). SDL frees a window's
// surface on the next SDL_GetWindowSurface after a resize and with the window;
// it frees every window when video shuts down.
void PySDL_WindowSurfaceForget(PySDL_Surface *surface);  // from Surface tp_dealloc
void PySDL_WindowSurfaceDrop(SDL_Window *window);        // SDL is about to free `window`'s surface
void PySDL_InvalidateWindows(void);   // video is gone: null every Window / Renderer / Texture / Cursor, empty every window surface
void PySDL_CheckVideoGone(void);      // PySDL_InvalidateWindows() if video is no longer running

// Cursor tracking (pysdl_Cursor.c): SDL_DestroyCursor frees a cursor other
// (borrowed) wrappers may hold, and video shutdown frees every cursor.
void PySDL_InvalidateCursors(void);   // from PySDL_InvalidateWindows

// SDL forgets Python callbacks behind our back: stopping the event subsystem
// drops the event filter and watchers (pysdl_Events.c), SDL_Quit drops every
// hint callback (pysdl_Video.c). pysdl.c's Quit / QuitSubSystem call these.
void PySDL_EventWatchesGone(void);
void PySDL_HintCallbacksGone(void);

// Timer callbacks vs. shutdown (pysdl_Timer.c): around SDL_Quit /
// SDL_QuitSubSystem, timer callbacks skip their Python call, and the hold
// first waits (GIL released) for one already running. Call with the GIL held.
void PySDL_TimersHold(void);
void PySDL_TimersRelease(void);

// Renderer / texture tracking (pysdl_Renderer.c). SDL_DestroyRenderer frees
// every texture of that renderer, SDL_DestroyWindow destroys the window's
// renderer, and video shutdown (SDL_QuitRender) frees every renderer; each
// Texture wrapper records its renderer so those paths can invalidate it.
int  PySDL_TextureTrack(PySDL_Texture *texture, SDL_Renderer *renderer);  // after creating ->texture
void PySDL_TextureForget(PySDL_Texture *texture);                         // from Texture tp_dealloc
void PySDL_TextureInvalidate(PySDL_Texture *texture);                     // pysdl_Texture.c
void PySDL_RendererWindowGone(SDL_Window *window);  // before SDL_DestroyWindow: destroy its renderer first
PyObject * PySDL_RendererFor(SDL_Renderer *renderer);  // the owning Renderer (new ref), or None
int  PySDL_RendererOwned(SDL_Renderer *renderer);      // 1 if a Renderer wrapper owns `renderer`
void PySDL_InvalidateRenderers(void);                  // video is gone: null every Renderer and Texture

// Bytes SDL reads or writes for a w x h image of `format` with `*pitch` bytes
// per row (0 = tightly packed: filled in), including the chroma planes of a
// planar YUV format. -1 with a ValueError for a pitch shorter than a row, a
// format without a byte layout, or an overflow.
Py_ssize_t PySDL_PixelsSize(SDL_PixelFormat format, int w, int h, int *pitch);

#endif // __PYSDL_H__
