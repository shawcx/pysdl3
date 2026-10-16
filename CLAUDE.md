# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A CPython C extension that exposes SDL3 (plus SDL3_image, and SDL3_ttf when
available) to Python 3 as a single module named `SDL3`. It is a thin, hand-written binding: only the subset of SDL
the author needed is wrapped, and wrapped functions/methods drop the `SDL_`
prefix (`SDL_GetPlatform` → `SDL3.GetPlatform`, `SDL_RenderPresent` →
`Renderer.Present`). Constant names follow SDL3 minus the `SDL_` prefix
(`SDL_EVENT_KEY_DOWN` → `EVENT_KEY_DOWN`, `SDLK_A` → `K_A`).

The codebase started as an SDL2 binding; `docs/SDL3-MIGRATION.md` records the
port. `docs/ROADMAP.md` holds the phase history and the style guide.

## Build & install

Needs Python 3.10+ (`python_requires`; `Py_NewRef`, plus a `Py_IsFinalizing`
shim in `pysdl.h` below 3.13) and SDL 3.2+. System dependencies first:
- macOS: `brew install sdl3 sdl3_image` (+ `sdl3_ttf` for text)
- Debian 13+ / Ubuntu 25.10+: `apt install build-essential pkg-config python3-dev
  python3-venv libsdl3-dev libsdl3-image-dev` (+ `libsdl3-ttf-dev`)

Then, in a venv (Homebrew and Debian Pythons refuse a bare `pip install`, PEP 668;
on macOS plain `python3` may be Xcode's 3.9, so create it with Homebrew's):
- `pip install .` — build (`pyproject.toml` → setuptools backend, which runs
  `setup.py`) and install the extension
- `python3 setup.py build` — compile in place under `build/` without installing
- `python3 -m build` — sdist + wheel into `dist/` (`pip install build`);
  `MANIFEST.in` bundles `src/` into the sdist
- `make deb` — Debian package `python3-sdl3` via `dpkg-buildpackage -us -uc -b`
  (needs `sudo apt build-dep .`; the `.deb` lands in `../`). `debian/` is
  debhelper 13 + pybuild with `pybuild-plugin-pyproject`, native format
  (version from `debian/changelog` — bump it with `setup.py`); the build runs
  the test suite, and `debian/tests/control` is an autopkgtest smoke test of
  the installed package. Replaced stdeb.

`setup.py` probes `pkg-config sdl3 sdl3-image` (seeding `PKG_CONFIG_PATH` with the
Homebrew keg dirs — SDL3 ships no `sdl3-config`) and always appends `/usr/local`
and `/opt/homebrew` as a fallback. All of `src/*.c` is globbed into one
`Extension`, so a new `.c` file is picked up automatically.

Satellite libraries are **optional**: `setup.py`'s `OPTIONAL` table lists
(pkg-config name, library, define, opt-out env var); each is linked, with its
define (e.g. `PYSDL_HAVE_TTF`), when `pkg-config` finds it or, failing that,
its header `<library>/SDL_<name>.h` is in an include dir. `PYSDL_TTF=0 python3
setup.py build` forces a build without SDL3_ttf. Everything for one lives in
its own file inside `#ifdef PYSDL_HAVE_<LIB>` (plus its include and type
declarations in `pysdl.h` and its registration in `PyInit_SDL3`).

`src/_constants.c` is **generated** — `python3 tools/gen_constants.py --generate`
emits the whole file from the installed SDL3 headers (curated header allowlist +
blocklist + `_GEN_EXTRA` synthesized constants, all in the script). Regenerate it
rather than hand-editing; `--emit <substr>` still prints pasteable lines for
ad-hoc lookups. Constants missing from SDL 3.2.0 get an
`#if SDL_VERSION_ATLEAST(...)` guard: the script diffs the headers of each
release in `_GEN_RELEASES` (fetched once into `~/.cache/pysdl3-headers`); add a
release there when a new SDL minor version ships.

## Tests

`tests/` is a headless pytest suite (`pip install pytest`, then `python3 -m
pytest`). `tests/conftest.py` forces the `dummy` video/audio drivers, (re)builds the
extension when `build/` has none for this interpreter or `src/` is newer, and
puts the newest one on `sys.path`; the `sdl` fixture
does one `Init`/`Quit` per session. Run one file with `python3 -m pytest
tests/test_audio.py`. In a venv the auto-build needs `setuptools` too
(`pip install pytest setuptools`). `tests/test_ttf.py` skips itself when the
build has no SDL3_ttf or no TrueType font can be found (it tries DejaVu, Arial,
then `fc-match`); its `ttf` fixture does `TTF_Init` / `TTF_Quit` per test.

CI (`.github/workflows/tests.yml`) runs on push / PR. In a `debian:trixie`
container (SDL 3.2, Python 3.13 — GitHub's Ubuntu images have no libsdl3) it
builds the Debian package (which runs the suite), lints it with lintian, runs
the autopkgtest and uploads the `.deb` as an artifact; on macOS it runs the
suite with Homebrew SDL at Python 3.10 and 3.14.

`example/` holds runnable example programs, most of which open a window and need
a display:
- `python3 example/info.py` — prints CPU/display/renderer info, no window
- `python3 example/draw.py [out.png]` — primitives, blend modes, render-to-texture,
  geometry; with a path it saves a screenshot (`docs/screenshot.png`) and exits
- `python3 example/surface.py [out.png]` — software-surface compositing, no window
- `python3 example/keyboard.py` — keyboard / text-input / mouse event echo, cursors
- `python3 example/gamepad.py [--virtual | --demo]` — gamepad / joystick monitor:
  device info, bindings, touchpad / gyro / battery; `--virtual` adds a virtual
  pad, `--demo` drives it unattended (runs headless)
- `python3 example/events.py [--seconds N]` — dump every event, custom + cross-thread
  events, filter, event watcher, hint callback; `--seconds N` quits by itself (runs
  headless)
- `python3 example/timer.py` — fixed-rate animation driven by SDL3.Timer + power state
- `python3 example/window.py` — window-state playground: border/grab/opacity/flash, message box, display + Vulkan info
- `python3 example/wav.py [file.wav]` — LoadWAV + AudioStream resample + queue playback
- `python3 example/rects.py` — live rect intersection / union / enclose / line-clip
- `python3 example/image.py [file]` — SDL_image: format probes, animations, SVG / XPM,
  encode to bytes; no window
- `python3 example/logical.py [--frames N]` — logical-presentation canvas: mouse
  picking, RenderGeometryRaw, LockToSurface, vsync / mode toggles; `--frames N`
  exits after N frames (runs headless)
- `python3 example/text.py [--font PATH] [--frames N]` — SDL3_ttf render modes,
  styles, wrapped text, metrics (needs the SDL3_ttf build)
- `python3 example/simple.py <image>` — load an image, show it, event loop
- `python3 example/audio.py` — audio callback + OpenGL visualizer (also needs a `pygl` module)
- `example/adjust.py` — fullscreen test pattern on every display

## Architecture

`src/pysdl.h` is the shared header: it declares every wrapper struct, its
`PyTypeObject`, the module-wide `pysdl_Error` exception, and the helpers in
`pysdl_util.c`. Every `.c` file includes only this.

- `src/pysdl.c` — module definition and `PyInit_SDL3`. Holds `Init` /
  `InitSubSystem` / `Quit`, `LoadBMP`, display / GL / CPU / audio-device
  queries, error / clipboard / screensaver, surface & blend-mode factories,
  `ConvertPixels` / `PremultiplyAlpha`.
  `_add_type()` registers each constructible wrapper (and `Gamepad` also gets a
  `GameController` alias).
- `src/pysdl_Events.c` — the event queue: `_event()` (an `SDL_Event` ->
  `(type, data)` converter; `data` is a tuple for structured events, a bare
  `str` for `EVENT_TEXT_INPUT`, `None` for `EVENT_QUIT` and unknown types), plus
  `PollEvent` / `WaitEvent` / `PushEvent` / `PeepEvents` / the filter /
  event watchers etc.
  `EVENT_WINDOW_*` / `EVENT_DISPLAY_*` are handled as per-type ranges
  (`(type, data1, data2, id)`); SDL3 owns `drop.data` so there is no `consume`
  flag any more. Device events lead with `which` (the instance id): gamepad
  touchpad -> `(which, touchpad, finger, x, y, pressure)`, gamepad sensor ->
  `(which, sensor, (x, y, z), sensor_timestamp_ns)`, joystick battery ->
  `(which, POWERSTATE_*, percent)`, added / removed / remapped / update-complete
  / Steam-handle -> `(which,)`.
- `src/pysdl_Window.c`, `pysdl_Renderer.c`, `pysdl_Surface.c`, `pysdl_Texture.c`,
  `pysdl_Audio.c`, `pysdl_AudioStream.c`, `pysdl_PixelFormat.c`, `pysdl_Palette.c`,
  `pysdl_Cursor.c`, `pysdl_Joystick.c`, `pysdl_Gamepad.c`, `pysdl_Timer.c`,
  `pysdl_Haptic.c`, `pysdl_Sensor.c`, `pysdl_Font.c` — one wrapped SDL object
  per file, each a full `PyTypeObject` with `PySDL_<Type>_<Method>` functions.
  `pysdl_Font.c` (SDL3_ttf, only under `PYSDL_HAVE_TTF`) also holds the `TTF_*`
  module functions (`pysdl_ttf_methods`) and registers the `TTF_*` constants
  (`PySDL_TTF_Constants`), which the SDL3-core generator doesn't scan.
- `src/pysdl_Input.c` — module-level keyboard / mouse / touch / text-input
  functions. Its own `PyMethodDef` array (`pysdl_input_methods`) is merged into
  the module in `PyInit_SDL3` with `PyModule_AddFunctions`; `pysdl_Events.c`,
  `pysdl_Video.c` (extra display queries, message boxes, hints and hint
  callbacks, `OpenURL` /
  locales, GL / Vulkan loaders, `GetWindowFromID` / `GetGrabbedWindow`,
  `CreateWindowAndRenderer`),
  `pysdl_Audio.c` (drivers, `LoadWAV`, `MixAudioFormat`, device-format queries),
  `pysdl_Rect.c` (rect / point geometry), `pysdl_Image.c` (SDL_image:
  `LoadImage`, `IMG_is*`, `IMG_LoadAnimation` / `IMG_SaveAnimation`, SVG / XPM,
  `IMG_Version`; also `PySDL_IMG_LoadTexture` behind `Renderer.LoadTexture`),
  `pysdl_Cursor.c`, `pysdl_Joystick.c`, `pysdl_Gamepad.c`, `pysdl_Haptic.c`,
  `pysdl_Sensor.c` do the same for their functions. Use this pattern to add a
  batch of module functions from a new file.
- `src/pysdl_util.c` — `PySDL_New()` (wrapper allocation), `PySDL_ThreadEnter` /
  `PySDL_ThreadLeave` (GIL handling for SDL-owned threads), and the
  `PyToRect` / `PyToPoint` / `PyToColor` / `PyToFRect` / `PyToFPoint` /
  `PyToPixel` / `RectToPy` / `PointToPy` converters, plus the `SDL_IOStream`
  helpers: `PySDL_PathFromObject` (str / os.PathLike -> str path; bytes is
  never a path), `PySDL_IOFromObject` (a path or any bytes-like -> readable
  stream; release the returned `Py_buffer` after SDL closes it) and
  `PySDL_SaveOpen` / `PySDL_SaveClose` (a path or `None` -> file or growable
  `SDL_IOFromDynamicMem` stream -> `None` or the written `bytes`).
  `PySDL_PixelsSize(format, w, h, &pitch)` gives the bytes SDL reads / writes
  for an image (pitch 0 = tight; YUV chroma planes included) to check caller
  buffers against; `PySDL_SurfaceArg` takes a Surface argument and rejects an
  uninitialised one where SDL would read NULL as "none".
- `src/_constants.c` — **generated** by `tools/gen_constants.py --generate`;
  `_constants(module)` registers ~1050 SDL3 enum/#define values as module int
  constants. Do not hand-edit: change the header allowlist / blocklist /
  `_GEN_EXTRA` in the script and regenerate. The binding targets SDL 3.2+;
  constants newer than 3.2.0 are generated inside `SDL_VERSION_ATLEAST`
  guards, and everything else must compile against 3.2 (CI's Debian job
  checks).

### Object model

Each wrapper struct is `PyObject_HEAD` plus one raw SDL pointer/handle (plus
tracking fields where needed: `Surface`/`Window`/`Cursor`.`shouldFree`,
`Renderer.target`, `Renderer.surface`, `Texture.locked`, `Texture.renderer`,
`Audio.pycallback`, `Surface.pixels`). The SDL pointer is
filled in either by `tp_init` (public construction) or afterwards by the C code
that allocated the wrapper with `PySDL_New(&PySDL_X_Type)`; it is released in
`tp_dealloc` unless `shouldFree` is 0 (a *borrowed* pointer SDL still owns —
`Window.GetWindowSurface()`, `GetKeyboardFocus()`/`GetMouseFocus()` via
`PySDL_WrapWindow()`, `GetDefaultCursor()`, `Texture.LockToSurface()`).
`GetCursor()` returns the owning `Cursor` object when the current cursor is one
of ours, so only SDL's default cursor is ever borrowed. Joysticks and
gamepads are never borrowed: SDL refcounts opens of the same id, so
`Gamepad.GetJoystick()` and the `*FromID` / `*FromPlayerIndex` lookups re-open
the device and return a new owned wrapper (`None` if nothing is open). Nor are
renderers: each SDL renderer has one `Renderer` wrapper, and
`Window.GetRenderer()` returns that same object.

`Window`, `Audio`, `AudioStream`, `Renderer`, `Texture`, `PixelFormat`,
`Palette`, `Cursor`, `Joystick`, `Gamepad`, `Timer`, `Haptic`, and `Sensor` are
in the module namespace and constructible: `SDL3.Window(title=None, size=…, …)`,
`SDL3.Renderer(window, name=None)`, `SDL3.Texture(renderer, …)`,
`SDL3.PixelFormat(format_enum)`, `SDL3.Palette(ncolors)`,
`SDL3.Cursor(system_cursor_id)`, `SDL3.Joystick(instance_id)`,
`SDL3.Gamepad(instance_id)` (aliased `GameController`),
`SDL3.Timer(interval_ms, callback)`, `SDL3.Haptic(instance_id)`,
`SDL3.Sensor(instance_id)`,
`SDL3.AudioStream(src_format, src_channels, src_rate, dst_…)`, and with SDL3_ttf
`SDL3.Font(path_or_bytes, ptsize=12.0, index=0, *, hdpi, vdpi)`. SDL3 identifies
joystick / gamepad / haptic / sensor devices by an `SDL_*ID` (from
`GetJoysticks()` / `GetHaptics()` / `GetSensors()`), not a device index. Every
`tp_init` takes its primary arg as *optional* — with none given it just nulls
the pointer, the path `PySDL_New` and the C-side factory functions use.
`Window`/`Renderer`/`Texture` lean on SDL's NULL-pointer tolerance (except
where SDL dereferences unchecked: `SDL_FlushRenderer`, and SDL 3.2's
`SDL_SetRenderTarget`, go through `pysdl_Renderer.c`'s `_valid()`); the rest go
through a `_fmt()`/`_pal()`/`_js()`/`_gp()`/`_h()`/`_s()`/`_font()` guard that raises
on an uninitialised instance. `Surface` is still constructed only via
module/Window/Renderer functions.

`PixelFormat` no longer wraps a single `SDL_PixelFormat *` struct (SDL3 split it
into an enum plus a cached, never-freed `SDL_PixelFormatDetails`); the wrapper
holds `{ SDL_PixelFormat format; const SDL_PixelFormatDetails *details;
PyObject *palette; }` where `palette` is a kept-alive `SDL3.Palette` from
`SetPalette()`.

`Font` keeps two extra fields: `data`, a `Py_buffer` pinning the font file's
bytes (SDL_ttf reads the stream lazily for the font's whole life, so the buffer
is released only after `TTF_CloseFont`, which closes the autoclose stream), and
`session`. `TTF_CloseFont` after the last `TTF_Quit` touches freed FreeType
state, so the `TTF_Quit` wrapper advances a module-level session counter when
SDL_ttf actually shuts down (its init is counted); a font from an older session
raises on use and is dropped without being closed. Text crosses as UTF-8 with
an explicit length (NUL characters refused); `Measure` converts SDL's byte count
back to characters. The GIL stays held: a `TTF_Font` is not thread-safe.

`SDL3.Timer(interval, callback)` registers `SDL_AddTimer`; the callback runs on
SDL's timer thread through the `PySDL_ThreadEnter` trampoline and returns the
next interval (`None` = same, `0` or falsy = stop). `Timer.Remove()` — and
dropping the wrapper, which calls it from `tp_dealloc` — cancels the timer.
`SDL_RemoveTimer` does **not** wait for a callback already under way, so SDL is
never handed a Python object: its `param` is an integer token, the trampoline
looks the callable up in a token → callable dict under the GIL, and `Remove()`
deletes the entry first — a late callback finds nothing and returns 0. (Passing
the callable itself segfaulted when a timer was dropped mid-callback.) Because
`Remove()` doesn't wait, `Quit` / `QuitSubSystem` do: see Threading.

`PySDL_Surface` carries a `shouldFree` flag: surfaces it owns (loaded images,
`CreateSurface`) are `SDL_DestroySurface`d on dealloc; a borrowed surface like
`Window.GetWindowSurface()` sets `shouldFree = 0`. It also holds a `Py_buffer
pixels` (`pixels.obj == NULL` unless it is a `…SurfaceFrom` surface) that keeps
the caller's buffer alive for the surface's lifetime — SDL does not copy it.
`surface->format` is the `SDL_PixelFormat` enum; look details up with
`SDL_GetPixelFormatDetails(surface->format)` and the palette with
`SDL_GetSurfacePalette(surface)`.

Window surfaces are tracked so they never dangle (`pysdl_Window.c`, on the
`PySDL_Registry` helper in `pysdl_util.c`): `GetWindowSurface()` returns one
`Surface` per window (`window_id` set, shared across wrappers), and when SDL
frees it — replaced on the next `GetWindowSurface()` after a resize, or with
the window — `PySDL_SurfaceDetach` points the wrapper at an empty 0×0 surface
of its own. Every `Window` wrapper is registered too: destroying a window nulls
its borrowed wrappers, and `Quit()` / a `QuitSubSystem()` that stops video
(`PySDL_CheckVideoGone`) nulls every `Window` and empties every window surface.
Getters such as `Surface.w` read the SDL struct directly (they raise on an
uninitialised Surface), so anything that lets SDL free a surface under a
wrapper must detach it first. Destroying a window also destroys its child
windows (`SetWindowModalFor` parents them): the Window's dealloc handles the
whole subtree.

Renderers and textures are tracked the same way (`pysdl_Renderer.c`): every
`Renderer` wrapper registers in `tp_init`, every Texture records its
`SDL_Renderer` and registers via `PySDL_TextureTrack()` at each creation site
(`Texture(...)`, `CreateTextureFromSurface`, `LoadTexture` — a new site must
call it too). SDL frees a renderer's textures when the renderer goes, so each
path invalidates them first (`PySDL_TextureInvalidate`: pointer nulled,
`LockToSurface` Surface emptied, methods raise): the Renderer's dealloc; a
Window's dealloc, which destroys the window's Renderer itself first
(`PySDL_RendererWindowGone` — `SDL_DestroyWindow` would leave a husk that
`SDL_FlushRenderer` doesn't check) and nulls that wrapper; and video shutdown,
whose `SDL_QuitRender` frees every renderer (`PySDL_InvalidateRenderers`, from
`PySDL_InvalidateWindows`; `QuitSubSystem` only checks if video was running, as
a software renderer on a Surface needs none). SDL calls that read a NULL
texture as "none" (`SetRenderTarget`, `RenderGeometry*`) take Texture args
through `_texture_arg()`, which raises. `CreateSoftwareRenderer(surface)` keeps
its Surface alive in `Renderer.surface` (SDL holds the pointer) and refuses
SDL-owned ones; destroying a software renderer on a window frees the window
surface, so `_destroy()` detaches that first.

Cursors too (`pysdl_Cursor.c`): every `Cursor` wrapper registers in `tp_init`.
An owning Cursor's dealloc nulls other wrappers of the same cursor before
`SDL_DestroyCursor`, and `PySDL_InvalidateWindows()` calls
`PySDL_InvalidateCursors()`, because video shutdown frees every cursor - ours
included, which must then not be freed again. A NULL cursor raises in
`Cursor.Set` / `SetCursor` (`SDL_SetCursor(NULL)` means "redraw"), and cursors
can't be created while video is down. SDL only compares stale cursor pointers
against its list, so the hazard is acting on a cursor that reused the address.

### Threading

Blocking SDL calls (`SDL_LoadBMP_IO`, the `IMG_Load*` / `IMG_is*` loaders and
probes, `SDL_IOFromFile`, `SDL_WaitEvent`, `SDL_GL_SwapWindow`, `SDL_Delay`) are
wrapped in `Py_BEGIN_ALLOW_THREADS` / `Py_END_ALLOW_THREADS`. Saves keep the GIL
while SDL encodes (only the file open / close drop it), since the encoder reads
a surface another thread could otherwise free. This is **mandatory** for any SDL call that can block on
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

The GIL rule also covers SDL's **event-watcher lock**: the event filter and the
watchers run under it, on whichever thread pushed the event (`SDL_SetEventFilter`
/ `SDL_FilterEvents` also hold the queue lock), and then take the GIL - which
the Python callback may hand to another thread mid-call. So every call that
pushes, pumps, polls, peeks or flushes events, or (un)registers a filter /
watch (`PollEvent`, `PumpEvents`, `PushEvent`, `FlushEvent(s)`, `HasEvent(s)`,
`EventState`, `QuitRequested`, `SetEventFilter`, `FilterEvents`,
`Add/RemoveEventWatch`, `ResetKeyboard`, the clipboard setters) drops the GIL
around the SDL call; holding it there deadlocks against a callback on another
thread. SDL functions that push events only as a side effect (most window
calls) still hold the GIL; that residual risk needs a filter / watcher and a
concurrent push from another thread. Hint callbacks run under SDL's hint lock,
so the hint calls (`Set/GetHint`, `ResetHint(s)`, `AddHintCallback`, ...) drop
the GIL as well; since nearly every SDL call reads hints, keep hint callbacks
short.

**SDL's joystick lock** works the same way: SDL runs the virtual-joystick
callbacks holding it. Every Python entry point of `pysdl_Joystick.c` /
`pysdl_Gamepad.c` takes it first with the GIL released
(`PySDL_JoystickLock()`, a no-op while the joystick subsystem is down) and runs
holding both, so no thread waits for that lock while holding the GIL. New
functions there are declared with `PYSDL_JOYSTICK_LOCKED(name, selftype)` /
`PYSDL_JOYSTICK_LOCKED_KW` (pysdl.h) in place of a prototype, and defined as
`name##_impl`; `tp_init` / `tp_dealloc` (open / close) and any other file's
joystick call (`pysdl_Haptic.c`) use `PySDL_JoystickLock()` /
`PySDL_JoystickUnlock()` directly.

`Init` / `InitSubSystem` / `QuitSubSystem` / `Quit` drop the GIL around SDL
too: shutting a subsystem down joins or waits for SDL threads that may be
inside a Python callback waiting for the GIL (`SDL_QuitTimers` joins the timer
thread, `SDL_QuitAudio` closes devices under their callbacks, joystick init /
quit take the joystick lock). Their bookkeeping (`PySDL_CheckVideoGone`,
`PySDL_InvalidateWindows`, the `...Gone()` hooks) runs with the GIL held,
before and after. `Quit` / `QuitSubSystem` also bracket SDL with
`PySDL_TimersHold()` / `PySDL_TimersRelease()`: SDL stops the timer thread
last, so a Timer callback could otherwise still be using a subsystem (pushing
an event) while it is destroyed; held callbacks skip their Python call, and the
hold waits for one already running.

New off-main-thread callbacks (timers, event filters) must follow the same
`PySDL_ThreadEnter` / `PySDL_ThreadLeave` pattern, take their own reference to
the callable for the call (a callback may unregister itself), and never hand
SDL a pointer to a Python object it may outlive:
- event filter: SDL's userdata is `NULL`; the trampoline reads
  `_py_event_filter` under the GIL (`pysdl_Events.c`);
- event watchers: integer tokens -> callables in `_py_event_watches`;
- hint callbacks: one SDL callback per hint name (userdata `NULL`, never
  removed - SDL's list walk only survives a callback removing itself), the
  callables in `_hint_callbacks` (name -> list, `pysdl_Video.c`);
- timers and virtual joysticks: integer tokens (`SDL3.Timer` above, virtual
  joysticks below).
SDL forgets some of these behind our back: stopping the event subsystem drops
the filter and watchers, `SDL_Quit` drops every hint callback, so `Quit` /
`QuitSubSystem` (`pysdl.c`) call `PySDL_EventWatchesGone()` /
`PySDL_HintCallbacksGone()`.

Virtual-joystick callbacks (`JoystickAttachVirtualEx(..., rumble=fn, ...)`,
`pysdl_Joystick.c`) use the Timer's token design: SDL's userdata is an integer
token, the callables live in a token-keyed dict entry made *before* the attach
(SDL calls `set_player_index` from inside it), and SDL's `Cleanup` callback
drops the entry when the device goes (detach, subsystem quit, failed attach).
SDL runs them with its joystick lock held (hence the lock rule above),
sometimes from a dealloc (`SDL_CloseJoystick` stops a rumble), so the trampoline
sets any pending exception aside (`PyErr_Fetch` / `PyErr_Restore`) around the
call — do the same in any callback SDL can fire from inside another SDL call
(the event filter / watcher and hint trampolines do).

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
  `if (arg && arg != Py_None)` since an omitted optional stays `NULL`. Some SDL
  calls trust rect sizes (`SDL_StretchSurface`, `SDL_LockTexture`, the
  YUV / NV texture updates when the clip is empty in one direction), so a
  `-1` from a 2-item rect must not reach them: clip or check first.
- Check caller buffers against everything SDL will read before calling it
  (`PySDL_PixelsSize`, the plane checks in `Texture.UpdateYUV` / `UpdateNV`,
  the stride / alignment checks in `RenderGeometryRaw`).
- GUIDs cross into Python as 32-char hex strings: `GUIDToPy(SDL_GUID)`, and
  `PyToGUID(obj, &guid)` (accepts that str or the raw 16 bytes; 0 + exception on
  bad input), both in `pysdl_Joystick.c`, declared in `pysdl.h`.
- Anything that reads a file (`LoadBMP`, `LoadImage`, `Renderer.LoadTexture`,
  `IMG_*`) takes a path (str / os.PathLike) or the file's bytes through
  `PySDL_IOFromObject`; a bytes source is pinned only for the call, since SDL
  reads everything before returning. Anything that writes one
  (`Surface.Save*`, `IMG_SaveAnimation`) returns `bytes` when no path is given.
  SDL_image's own functions keep the `IMG_` prefix (`LoadImage` predates that).
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
