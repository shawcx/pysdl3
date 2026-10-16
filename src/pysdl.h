#ifndef __PYSDL_H__
#define __PYSDL_H__

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <structmember.h>

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3/SDL_vulkan.h>

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
    SDL_Renderer *renderer;
    PyObject *target;  // Texture currently set as render target, or NULL
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
    SDL_Joystick *joystick;
    int shouldFree;  // 0 when borrowed (Gamepad.GetJoystick)
} PySDL_Joystick;
extern PyTypeObject PySDL_Joystick_Type;

typedef struct {
    PyObject_HEAD
    SDL_Gamepad *gamepad;
} PySDL_Gamepad;
extern PyTypeObject PySDL_Gamepad_Type;

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

typedef struct {
    PyObject_HEAD
    SDL_AudioStream *stream;
} PySDL_AudioStream;
extern PyTypeObject PySDL_AudioStream_Type;

// generated constant table (tools/gen_constants.py --generate)
void _constants(PyObject *module);

// Extra module-function tables registered from their own files.
extern PyMethodDef pysdl_events_methods[];          // pysdl_events.c  (event queue)
extern PyMethodDef pysdl_input_methods[];           // pysdl_input.c   (keyboard / mouse / touch / text)
extern PyMethodDef pysdl_cursor_methods[];          // pysdl_Cursor.c
extern PyMethodDef pysdl_joystick_methods[];        // pysdl_Joystick.c
extern PyMethodDef pysdl_gamepad_methods[];         // pysdl_Gamepad.c
extern PyMethodDef pysdl_haptic_methods[];          // pysdl_Haptic.c
extern PyMethodDef pysdl_sensor_methods[];          // pysdl_Sensor.c
extern PyMethodDef pysdl_video_methods[];           // pysdl_video.c   (display / messagebox / hints / vulkan / metal)
extern PyMethodDef pysdl_audio_methods[];           // pysdl_Audio.c   (drivers / LoadWAV / mixing)
extern PyMethodDef pysdl_rect_methods[];            // pysdl_rect.c    (rect / point math)

//=========================================================
// Helpers (pysdl_util.c)
//=========================================================

// Allocate a wrapper instance of `type`; sets a TypeError and returns NULL on
// failure. Replaces the repeated PyObject_CallObject boilerplate.
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
void PySDL_InvalidateWindows(void);   // video is gone: null every Window, empty every window surface
void PySDL_CheckVideoGone(void);      // PySDL_InvalidateWindows() if video is no longer running

#endif // __PYSDL_H__
