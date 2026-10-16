# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A CPython C extension that exposes SDL3 (plus SDL3_image) to Python 3 as a single
module named `SDL3`. It is a thin, hand-written binding: only the subset of SDL
the author needed is wrapped, and wrapped functions/methods drop the `SDL_`
prefix (`SDL_GetPlatform` → `SDL3.GetPlatform`, `SDL_RenderPresent` →
`Renderer.Present`). Constant names follow SDL3 minus the `SDL_` prefix
(`SDL_EVENT_KEY_DOWN` → `EVENT_KEY_DOWN`, `SDLK_A` → `K_A`).

The codebase started as an SDL2 binding; `docs/SDL3-MIGRATION.md` records the
port. `docs/ROADMAP.md` holds the phase history and the style guide.

## Build & install

System dependencies must be present first:
- macOS: `brew install sdl3 sdl3_image`
- Debian/Ubuntu (Trixie+): `apt install libsdl3-dev libsdl3-image-dev`

Then:
- `pip install .` — build and install the extension
- `python3 setup.py build` — compile in place under `build/` without installing
- `python3 setup.py sdist` — source tarball; `MANIFEST.in` bundles `src/` so the
  Debian package build (`stdeb`/`deb_dist`, see `.gitignore`) can compile from it

`setup.py` probes `pkg-config sdl3 sdl3-image` (seeding `PKG_CONFIG_PATH` with the
Homebrew keg dirs — SDL3 ships no `sdl3-config`) and always appends `/usr/local`
and `/opt/homebrew` as a fallback. All of `src/*.c` is globbed into one
`Extension`, so a new `.c` file is picked up automatically.

`src/_constants.c` is **generated** — `python3 tools/gen_constants.py --generate`
emits the whole file from the installed SDL3 headers (curated header allowlist +
blocklist + `_GEN_EXTRA` synthesized constants, all in the script). Regenerate it
rather than hand-editing; `--emit <substr>` still prints pasteable lines for
ad-hoc lookups.

## Tests

`tests/` is a headless pytest suite (`pip install pytest`, then `python3 -m
pytest`). `tests/conftest.py` forces the `dummy` video/audio drivers, builds the
extension if `build/` is missing, and puts it on `sys.path`; the `sdl` fixture
does one `Init`/`Quit` per session. Run one file with `python3 -m pytest
tests/test_audio.py`.

`example/` holds runnable example programs, most of which open a window and need
a display:
- `python3 example/info.py` — prints CPU/display/renderer info, no window
- `python3 example/draw.py [out.png]` — primitives, blend modes, render-to-texture,
  geometry; with a path it saves a screenshot (`docs/screenshot.png`) and exits
- `python3 example/surface.py [out.png]` — software-surface compositing, no window
- `python3 example/keyboard.py` — keyboard / text-input / mouse event echo, cursors
- `python3 example/gamepad.py [--virtual]` — gamepad / joystick monitor
- `python3 example/events.py` — dump every event, custom + cross-thread events, filter
- `python3 example/timer.py` — fixed-rate animation driven by SDL3.Timer + power state
- `python3 example/window.py` — window-state playground: border/grab/opacity/flash, message box, display + Vulkan info
- `python3 example/wav.py [file.wav]` — LoadWAV + AudioStream resample + queue playback
- `python3 example/rects.py` — live rect intersection / union / enclose / line-clip
- `python3 example/simple.py <image>` — load an image, show it, event loop
- `python3 example/audio.py` — audio callback + OpenGL visualizer (also needs a `pygl` module)
- `example/adjust.py` — fullscreen test pattern on every display

## Architecture

`src/pysdl.h` is the shared header: it declares every wrapper struct, its
`PyTypeObject`, the module-wide `pysdl_Error` exception, and the helpers in
`pysdl_util.c`. Every `.c` file includes only this.

- `src/pysdl.c` — module definition and `PyInit_SDL3`. Holds `Init` /
  `InitSubSystem` / `Quit`, `LoadImage`, display / GL / CPU / audio-device
  queries, error / clipboard / screensaver, surface & blend-mode factories.
  `_add_type()` registers each constructible wrapper (and `Gamepad` also gets a
  `GameController` alias).
- `src/pysdl_Events.c` — the event queue: `_event()` (an `SDL_Event` ->
  `(type, data)` converter; `data` is a tuple for structured events, a bare
  `str` for `EVENT_TEXT_INPUT`, `None` for `EVENT_QUIT` and unknown types), plus
  `PollEvent` / `WaitEvent` / `PushEvent` / `PeepEvents` / filters etc.
  `EVENT_WINDOW_*` / `EVENT_DISPLAY_*` are handled as per-type ranges
  (`(type, data1, data2, id)`); SDL3 owns `drop.data` so there is no `consume`
  flag any more.
- `src/pysdl_Window.c`, `pysdl_Renderer.c`, `pysdl_Surface.c`, `pysdl_Texture.c`,
  `pysdl_Audio.c`, `pysdl_AudioStream.c`, `pysdl_PixelFormat.c`, `pysdl_Palette.c`,
  `pysdl_Cursor.c`, `pysdl_Joystick.c`, `pysdl_Gamepad.c`, `pysdl_Timer.c`,
  `pysdl_Haptic.c`, `pysdl_Sensor.c` — one wrapped SDL object per file, each a
  full `PyTypeObject` with `PySDL_<Type>_<Method>` functions.
- `src/pysdl_Input.c` — module-level keyboard / mouse / touch / text-input
  functions. Its own `PyMethodDef` array (`pysdl_input_methods`) is merged into
  the module in `PyInit_SDL3` with `PyModule_AddFunctions`; `pysdl_Events.c`,
  `pysdl_Video.c` (extra display queries, message boxes, hints, `OpenURL` /
  locales, GL / Vulkan loaders, `GetWindowFromID` / `GetGrabbedWindow`),
  `pysdl_Audio.c` (drivers, `LoadWAV`, `MixAudioFormat`, device-format queries),
  `pysdl_Rect.c` (rect / point geometry), `pysdl_Cursor.c`, `pysdl_Joystick.c`,
  `pysdl_Gamepad.c`, `pysdl_Haptic.c`, `pysdl_Sensor.c` do the same for their
  functions. Use this pattern to add a batch of module functions from a new
  file.
- `src/pysdl_util.c` — `PySDL_New()` (wrapper allocation), `PySDL_ThreadEnter` /
  `PySDL_ThreadLeave` (GIL handling for SDL-owned threads), and the
  `PyToRect` / `PyToPoint` / `PyToColor` / `PyToFRect` / `PyToFPoint` /
  `PyToPixel` / `RectToPy` / `PointToPy` converters.
- `src/_constants.c` — **generated** by `tools/gen_constants.py --generate`;
  `_constants(module)` registers ~1050 SDL3 enum/#define values as module int
  constants. Do not hand-edit: change the header allowlist / blocklist /
  `_GEN_EXTRA` in the script and regenerate. The binding targets SDL 3.2+, so
  there are no version guards.

### Object model

Each wrapper struct is `PyObject_HEAD` plus one raw SDL pointer/handle (plus
tracking fields where needed: `Surface`/`Window`/`Cursor`/`Joystick`.`shouldFree`,
`Renderer.target`, `Audio.pycallback`, `Surface.pixels`). The SDL pointer is
filled in either by `tp_init` (public construction) or afterwards by the C code
that allocated the wrapper with `PySDL_New(&PySDL_X_Type)`; it is released in
`tp_dealloc` unless `shouldFree` is 0 (a *borrowed* pointer SDL still owns —
`Window.GetWindowSurface()`, `GetKeyboardFocus()`/`GetMouseFocus()` via
`PySDL_WrapWindow()`, `GetCursor()`/`GetDefaultCursor()`,
`Gamepad.GetJoystick()`).

`Window`, `Audio`, `AudioStream`, `Renderer`, `Texture`, `PixelFormat`,
`Palette`, `Cursor`, `Joystick`, `Gamepad`, `Timer`, `Haptic`, and `Sensor` are
in the module namespace and constructible: `SDL3.Window(title=None, size=…, …)`,
`SDL3.Renderer(window, name=None)`, `SDL3.Texture(renderer, …)`,
`SDL3.PixelFormat(format_enum)`, `SDL3.Palette(ncolors)`,
`SDL3.Cursor(system_cursor_id)`, `SDL3.Joystick(instance_id)`,
`SDL3.Gamepad(instance_id)` (aliased `GameController`),
`SDL3.Timer(interval_ms, callback)`, `SDL3.Haptic(instance_id)`,
`SDL3.Sensor(instance_id)`,
`SDL3.AudioStream(src_format, src_channels, src_rate, dst_…)`. SDL3 identifies
joystick / gamepad / haptic / sensor devices by an `SDL_*ID` (from
`GetJoysticks()` / `GetHaptics()` / `GetSensors()`), not a device index. Every
`tp_init` takes its primary arg as *optional* — with none given it just nulls
the pointer, the path `PySDL_New` and the C-side factory functions use.
`Window`/`Renderer`/`Texture` lean on SDL's NULL-pointer tolerance; the rest go
through a `_fmt()`/`_pal()`/`_js()`/`_gp()`/`_h()`/`_s()` guard that raises on an
uninitialised instance. `Surface` is still constructed only via
module/Window/Renderer functions.

`PixelFormat` no longer wraps a single `SDL_PixelFormat *` struct (SDL3 split it
into an enum plus a cached, never-freed `SDL_PixelFormatDetails`); the wrapper
holds `{ SDL_PixelFormat format; const SDL_PixelFormatDetails *details;
PyObject *palette; }` where `palette` is a kept-alive `SDL3.Palette` from
`SetPalette()`.

`SDL3.Timer(interval, callback)` registers `SDL_AddTimer`; the callback runs on
SDL's timer thread through the `PySDL_ThreadEnter` trampoline and returns the
next interval (`None` = same, `0` or falsy = stop). `Timer.Remove()` — and
dropping the wrapper, which calls it from `tp_dealloc` — cancels the timer (GIL
dropped around `SDL_RemoveTimer`, which joins the callback thread).

`PySDL_Surface` carries a `shouldFree` flag: surfaces it owns (loaded images,
`CreateSurface`) are `SDL_DestroySurface`d on dealloc; a borrowed surface like
`Window.GetWindowSurface()` sets `shouldFree = 0`. It also holds a `Py_buffer
pixels` (`pixels.obj == NULL` unless it is a `…SurfaceFrom` surface) that keeps
the caller's buffer alive for the surface's lifetime — SDL does not copy it.
`surface->format` is the `SDL_PixelFormat` enum; look details up with
`SDL_GetPixelFormatDetails(surface->format)` and the palette with
`SDL_GetSurfacePalette(surface)`.

### Threading

Blocking SDL calls (`SDL_LoadBMP`, `IMG_Load`, `SDL_WaitEvent`,
`SDL_GL_SwapWindow`, `SDL_Delay`) are wrapped in `Py_BEGIN_ALLOW_THREADS` /
`Py_END_ALLOW_THREADS`. This is **mandatory** for any SDL call that can block on
the audio callback thread (`SDL_DestroyAudioStream`, `SDL_LockAudioStream`):
that thread needs the GIL, so holding it here deadlocks.

SDL3 audio flows through a device-bound `SDL_AudioStream`.
`Audio.Open(..., callback=fn, userdata=obj)` wraps `SDL_OpenAudioDeviceStream`
and passes `(fn, userdata)` (a tuple, also kept in `pycallback`, released in
`Close`/dealloc) as the stream userdata. The C trampolines
`_playback_callback` / `_capture_callback` take
`(userdata, stream, additional, total)`, go through `PySDL_ThreadEnter` (which
bails if `Py_IsFinalizing()`), call the Python callable, and report any
exception with `PyErr_Print()`. Playback pushes the `bytes` the callback returns
with `SDL_PutAudioStreamData` (non-`bytes` / short → silence, not a crash);
recording drains the stream and hands the callback the `bytes`. Without a
callback, feed the stream with `Audio.Queue()` / drain it with `Audio.Dequeue()`.

New off-main-thread callbacks (timers, event filters) must follow the same
`PySDL_ThreadEnter` / `PySDL_ThreadLeave` pattern.

### Conventions

- Errors: set `pysdl_Error` (exposed as `SDL3.error`) with `SDL_GetError()` and
  return `NULL`. `pysdl_Renderer.c` / `pysdl_Texture.c` / `pysdl_Surface.c` have a
  local `_raise()` helper that does exactly this. SDL3 functions return `bool`
  (`true` = success); the idiom is `if (!SDL_X(...)) return _raise();` — but
  count-returning calls (`SDL_GetAudioStreamData`, `SDL_PeepEvents`) still return
  `int`, so check each call, not the type.
- Create wrapper objects with `PySDL_New(&PySDL_X_Type)`; `Py_DECREF` the wrapper
  on the SDL-failure path (it owns nothing yet, but the wrapper itself leaks).
- Method names drop `SDL_`. `Renderer` keeps the rest verbatim
  (`RenderSetViewport` — kept as the pre-SDL3 name even though the C call is now
  `SDL_SetRenderViewport`); `Texture` and `Surface` also drop the type word
  (`SDL_UpdateTexture` → `Update`, `SDL_SetSurfaceBlendMode` → `SetBlendMode`,
  `SDL_BlitSurface` → `Blit`) — except `Surface.LockSurface` / `UnlockSurface` /
  `SaveBMP`, kept for compatibility. New SDL3 names are often added as aliases
  next to the old ones (`RenderTexture`, `SetEventEnabled`, `GetJoysticks`).
- Take a `PySDL_<Type> *` argument with `O!` + `&PySDL_<Type>_Type`, or check
  with `PyObject_TypeCheck` before casting — never cast an unchecked `O`.
- Rect/point/colour args go through `PyToRect` / `PyToPoint` / `PyToColor` /
  `PyToFRect` / `PyToFPoint` (tuple or list; a 2-item rect leaves `w`/`h` as
  `-1`, a 3-item colour sets `a = 255`). Check the return — they raise and
  return 0 on bad input. Pass `None` for optional rect args; guard with
  `if (arg && arg != Py_None)` since an omitted optional stays `NULL`.
- `PyToPixel(obj, details, palette, &Uint32)` accepts an already-mapped int
  verbatim or maps an `(r,g,b[,a])` sequence through `details` (+ `palette`,
  which may be `NULL`) — used for fill/colour-key.
- Renderer draw primitives (`DrawPoint(s)`, `DrawLine(s)`, `DrawRect(s)`,
  `FillRect(s)`) call the SDL3 `SDL_RenderPoint/Line/Rect/FillRect` functions
  (float only) and accept int or float coordinates. `Copy` / `CopyEx` / `CopyF` /
  `CopyExF` are all aliases for `SDL_RenderTexture` / `SDL_RenderTextureRotated`
  (SDL3 dropped the integer-rect variants).
- Mouse / touch / wheel coordinates are `float` in SDL3; text input and relative
  mouse mode are per-`Window`.
- C99 designated initializers for `PyTypeObject`; `PY_SSIZE_T_CLEAN` is set.
