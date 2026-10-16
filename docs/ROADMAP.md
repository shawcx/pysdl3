# SDL binding roadmap

> **Now targeting SDL3.** Phases 0–9 below were completed against SDL2; the
> port to SDL3 (`SDL3` module, `import SDL3`) is tracked in
> [`SDL3-MIGRATION.md`](SDL3-MIGRATION.md). This file is kept for the phase
> history and the style guide, which still applies. Where a phase description
> names an SDL2 API, its SDL3 equivalent (or its removal) is recorded in the
> migration doc.

A phased plan that took this binding from partial coverage to broad SDL
(+ SDL_image) coverage, keeping the established style: one wrapped type per
`src/pysdl_<Type>.c` file, the `SDL_` prefix dropped from names, errors raised
through `pysdl_Error`, blocking calls wrapped in `Py_BEGIN_ALLOW_THREADS`.

## Design decisions to settle first

These shape every later phase.

| Question | Decision | Rationale |
|---|---|---|
| Event representation | Keep `(type, data)` with positional tuples; extend the `switch` in `_event()`. Add sub-type constants (`WINDOWEVENT_*`, `CONTROLLERBUTTON*`) and a `SDL2.event_name(type)` helper. | Consistency with existing `PollEvent`/`WaitEvent`. Attribute objects would be cleaner but a full rewrite. |
| Feeding file data to loaders | Accept `bytes` directly in `LoadBMP` / `LoadImage` / `LoadWAV` and build an `SDL_RWops` internally. Do not expose an `RWops` type. | Pythonic; avoids a whole type surface. |
| `_constants.c` upkeep | Add `tools/gen_constants.py` that scans the installed `SDL_*.h` for `#define`/enum values and emits the file (or a supplement). | ~700 hand-maintained constants will not keep pace with new subsystems. |
| Borrowed vs owned pointers | Generalize the `Surface.shouldFree` idea into a documented convention plus a `PYSDL_WRAP_BORROWED(type, ptr)` helper in `pysdl.h`. | Several new APIs return pointers SDL still owns (`GetWindowFromID`, `GameController.GetJoystick`, joystick-from-event). |
| Off-thread Python callbacks | One shared trampoline helper (GIL ensure -> `Py_IsFinalizing` guard -> call -> print/clear on error). Reuse for audio, timers, event filters, hint callbacks. | The audio callback pattern is about to be copied 3-4 times. |
| `setup.py` flags | Derive from `sdl2-config --cflags --libs` / `pkg-config`, keep the hardcoded homebrew / `/usr/local` list as fallback. | Removes the "edit setup.py" friction as more libs get linked. |

## Phase 0 - Infrastructure  — DONE

- **Headless test harness**: `pytest` suite under `tests/` (`conftest.py` forces
  the dummy drivers, auto-builds, injects `sys.path`; `sdl` session fixture).
  `example/info.py` ported to `tests/test_smoke.py`. 50 tests.
- **Audio callback template fixed**: `PySDL_Audio.pycallback` now holds the
  `(callback, userdata)` tuple and is released in `Close`/dealloc;
  `_playback_callback` checks the return is `bytes` and long enough (else
  silence); both callbacks route through `PySDL_ThreadEnter`. Also found & fixed
  a GIL deadlock — `Close`/`Lock`/`Pause` now drop the GIL around the SDL call
  that joins the callback thread.
- **Helpers** in `src/pysdl_util.c`: `PySDL_New`, `PySDL_ThreadEnter` /
  `PySDL_ThreadLeave`. (The `PYSDL_WRAP_BORROWED` macro is deferred to the first
  phase that returns a borrowed pointer; `Surface.shouldFree` remains the
  documented pattern.)
- **Struct converters** moved to `pysdl_util.c` and hardened (raise on bad
  input): `PyToRect` / `PyToPoint` / `PyToColor` / `RectToPy` / `PointToPy`.
  `Renderer.Copy` / `CopyEx` now check the return (they previously crashed on
  `Copy(texture)` with no rect — omitted optionals are `NULL`, not `Py_None`).
- **Constants tool** `tools/gen_constants.py`: reports the ~580 unexposed
  symbols grouped by header; `--emit <substr>` prints pasteable lines.
- **`setup.py`** now probes `sdl2-config` / `pkg-config`, keeping the hardcoded
  prefixes as fallback.

**Quick wins landed**: `Delay`, `GetTicks64` (guarded ≥2.0.18),
`GetPerformanceCounter`, `GetPerformanceFrequency`, `GetVersion`, `GetRevision`,
`SetError`, `ClearError`, `GetBasePath`, `GetPrefPath`, `EnableScreenSaver` /
`DisableScreenSaver` / `IsScreenSaverEnabled`, `GetClipboardText` /
`SetClipboardText` / `HasClipboardText`, `Renderer.GetRenderDrawColor`, and
`Texture.GL_Bind` now returns `(w, h)`.

## Phase 1 - Rendering completeness  — DONE

`pysdl_Renderer.c` / `pysdl_Texture.c` rewritten; `Renderer` and `Texture` are
now in the module namespace and constructible. Tests: `tests/test_renderer.py`,
`tests/test_texture.py`; example: `example/draw.py`.

- **Construction**: `SDL2.Renderer(window, index=-1, flags=0)`,
  `SDL2.Texture(renderer, format, access, size)`,
  `SDL2.CreateSoftwareRenderer(surface)`. `Window.CreateRenderer(flags=0,
  index=-1)` now takes args — **behaviour change**: it no longer forces
  `ACCELERATED | PRESENTVSYNC`; pass the flags you want.
- **Primitives**: `DrawPoint(s)`, `DrawLine(s)`, `DrawRect(s)`, `FillRect(s)`
  (all call the SDL `*F` functions internally, accept int or float),
  `RenderGeometry(texture, vertices, indices=None)` (≥2.0.18), `CopyF` / `CopyExF`.
- **State**: `Get/SetRenderDrawBlendMode`, `RenderSet/GetViewport`,
  `RenderSet/GetClipRect`, `RenderIsClipEnabled`, `RenderSet/GetScale`,
  `RenderSet/GetLogicalSize`, `RenderSet/GetIntegerScale`.
- **Targets**: `SetRenderTarget` / `GetRenderTarget` (returns the same wrapper
  object via `Renderer.target`), `RenderTargetSupported`,
  `RenderReadPixels(rect=None, format=PIXELFORMAT_ARGB8888)` -> bytes.
- **Info**: `GetRendererInfo`, `GetRendererOutputSize`, `RenderFlush`.
- **Texture**: `Update(pixels, rect=None, pitch=0)`, `UpdateYUV(...)`,
  `Set/GetColorMod`, `Set/GetAlphaMod`, `Set/GetBlendMode`, `Set/GetScaleMode`
  (≥2.0.12), `Lock(rect=None)` -> `(writable memoryview, pitch)` valid until
  `Unlock()`.
- `Copy` / `CopyEx` gained `O!` texture type-checking (was an unchecked cast).
- Module: `SDL2.ComposeCustomBlendMode(...)`.
- New converters `PyToFRect` / `PyToFPoint` in `pysdl_util.c`.
- Constants: `TEXTUREACCESS_*`, `TEXTUREMODULATE_*`, `BLENDMODE_*`,
  `BLENDOPERATION_*`, `BLENDFACTOR_*`, `SCALEMODE_*`.

## Phase 2 - Surfaces & pixels  — DONE

Tests: `tests/test_surface.py`, `tests/test_pixelformat.py`; example:
`example/surface.py`.

- **`Surface`** methods (short names, like `Texture`): `Blit`, `BlitScaled`,
  `SoftStretch`, `FillRect` / `FillRects` (colour is an int or `(r,g,b[,a])`),
  `SetColorKey` / `GetColorKey`, `SetBlendMode` / `GetBlendMode`,
  `SetColorMod` / `GetColorMod`, `SetAlphaMod` / `GetAlphaMod`,
  `SetClipRect` / `GetClipRect`, `SetRLE`, `SetPalette`, `Convert` (enum int
  *or* `PixelFormat`), `Duplicate`, `MapRGB` / `MapRGBA`, `GetPixelFormat`,
  `SavePNG`, `SaveJPG`. New `pitch` attribute.
- New **`SDL2.PixelFormat(format_enum)`** (`src/pysdl_PixelFormat.c`):
  `MapRGB` / `MapRGBA` / `GetRGB` / `GetRGBA`, `SetPalette`; attrs `format`,
  `bpp`, `bytes`, `R/G/B/Amask`.
- New **`SDL2.Palette(ncolors)`** (`src/pysdl_Palette.c`): `SetColors(colors,
  first=0)`, `GetColors()`, `ncolors`.
- Module: `CreateRGBSurfaceWithFormat` / `…WithFormatFrom`, `GetPixelFormatName`,
  `PixelFormatEnumToMasks`, `MasksToPixelFormatEnum`, `IMG_Init` / `IMG_Quit`.
- `LoadBMP` / `LoadImage` now also take the file's `bytes` (RWops built
  internally); `…SurfaceFrom` keeps the caller's buffer alive via
  `Surface.pixels` (`Py_buffer`) — fixes a pre-existing dangling-pointer bug.
- New converter `PyToPixel`. Constants: `PIXELFORMAT_{RGBA,ARGB,BGRA,ABGR}32`,
  `PIXELFORMAT_X{RGB,BGR}8888`, `SWSURFACE`/`PREALLOC`/`RLEACCEL`/`DONTFREE`,
  `IMG_INIT_*`.

## Phase 3 - Keyboard, mouse, text input  — DONE

New `src/pysdl_input.c` holds the module-level keyboard/mouse/text functions
(its `PyMethodDef` array is merged in `PyInit_SDL2` via `PyModule_AddFunctions`).
Tests: `tests/test_input.py`, `tests/test_cursor.py`; example:
`example/keyboard.py`.

- **Keyboard:** `GetKeyName`, `GetKeyFromName`, `GetScancodeName`,
  `GetScancodeFromName`, `GetKeyFromScancode`, `GetScancodeFromKey`,
  `SetModState`, `GetKeyboardFocus` (borrowed `Window`),
  `HasScreenKeyboardSupport`, `IsScreenKeyboardShown`. Constant `K_UNKNOWN`.
- **Text input:** `StartTextInput`, `StopTextInput`, `IsTextInputActive`,
  `SetTextInputRect`. `_event()` decodes `TEXTINPUT` (data = the `str`) and
  `TEXTEDITING` (`(text, start, length)`); the `KEYDOWN`/`KEYUP` tuple gained a
  5th element `repeat` (bool).
- **Mouse:** `GetMouseState` / `GetGlobalMouseState` / `GetRelativeMouseState`
  (each `(buttons, x, y)`), `WarpMouseInWindow`, `WarpMouseGlobal`,
  `SetRelativeMouseMode` / `GetRelativeMouseMode`, `CaptureMouse`,
  `GetMouseFocus` (borrowed `Window`). Constants `PRESSED`/`RELEASED`,
  `BUTTON_*` / `BUTTON_*MASK`, `MOUSEWHEEL_NORMAL`/`FLIPPED`.
- New **`SDL2.Cursor(system_cursor_id)`** (`src/pysdl_Cursor.c`) + `Set()`;
  module `CreateColorCursor(surface, hx, hy)`, `CreateCursor(data, mask, size,
  hot)`, `GetCursor` / `GetDefaultCursor` (borrowed), `SetCursor`. Constants
  `SYSTEM_CURSOR_*`.
- `Window.tp_init` now takes `title` as optional (borrowed-window path);
  `PySDL_Window` gained `shouldFree`. New helper `PySDL_WrapWindow()`.

## Phase 4 - Game controllers & joysticks  — DONE

Tests: `tests/test_joystick.py` (driven by a virtual joystick); example:
`example/gamepad.py`.

- New **`SDL2.Joystick(device_index)`** (`src/pysdl_Joystick.c`): `Name`,
  `GetGUID`, `InstanceID`, `Attached`, `NumAxes` / `NumButtons` / `NumHats` /
  `NumBalls`, `GetAxis` / `GetButton` / `GetHat` / `GetBall`, `Rumble`,
  `RumbleTriggers` / `SetLED` / `HasLED` (≥2.0.14), `CurrentPowerLevel`,
  `SetVirtualAxis` / `SetVirtualButton` / `SetVirtualHat` (≥2.0.14), `Close`.
  Module: `NumJoysticks`, `JoystickNameForIndex`, `JoystickUpdate`,
  `JoystickEventState`, `JoystickAttachVirtual` / `JoystickDetachVirtual` /
  `JoystickIsVirtual` (≥2.0.14).
- New **`SDL2.GameController(device_index)`** (`src/pysdl_GameController.c`):
  `Name`, `Attached`, `GetAxis`, `GetButton`, `Mapping`, `GetJoystick`
  (borrowed), `Rumble`, `RumbleTriggers` / `SetLED` (≥2.0.14), `GetType`
  (≥2.0.12), `Close`. Module: `IsGameController`, `GameControllerNameForIndex`,
  `GameControllerAddMapping` / `…AddMappingsFromFile`, `GameControllerUpdate`,
  `GameControllerEventState`.
- `_event()`: `JOYBALLMOTION`, `JOYHATMOTION`, `JOYDEVICEADDED/REMOVED`,
  `CONTROLLERAXISMOTION`, `CONTROLLERBUTTONDOWN/UP`,
  `CONTROLLERDEVICEADDED/REMOVED/REMAPPED`.
- Module: `InitSubSystem` / `QuitSubSystem` (were missing). Constants:
  `HAT_*`, `JOYSTICK_POWER_*`, `JOYSTICK_TYPE_*`, `CONTROLLER_AXIS_*`,
  `CONTROLLER_BUTTON_*`, `CONTROLLER_TYPE_*`, the joystick/controller event
  types.

## Phase 5 - Events subsystem completeness  — DONE

`_event()` and all the queue functions moved to the new `src/pysdl_events.c`
(`pysdl_events_methods` merged via `PyModule_AddFunctions`). `GetKeyState` /
`GetModState` moved from `pysdl.c` to `pysdl_input.c`. Tests:
`tests/test_events.py`; example: `example/events.py`.

- Module: `PumpEvents`, `PushEvent(type, code=0, windowID=0)`, `PeepEvents(count,
  action=PEEKEVENT, minType, maxType)` -> list, `FlushEvent` / `FlushEvents`,
  `HasEvent` / `HasEvents`, `WaitEventTimeout(ms)`, `RegisterEvents(n)`,
  `EventState(type, state)` / `GetEventState(type)`, `QuitRequested`.
- `_event()` now decodes `JOYDEVICE*` / `CONTROLLER*` (from phase 4) plus
  `DROPFILE` / `DROPTEXT` (frees `drop.file`) / `DROPBEGIN` / `DROPCOMPLETE`,
  `AUDIODEVICEADDED` / `REMOVED`, `FINGERDOWN` / `UP` / `MOTION`, `MULTIGESTURE`,
  `DOLLARGESTURE` / `DOLLARRECORD`, `DISPLAYEVENT`, `SENSORUPDATE` (≥2.0.9);
  any type `>= USEREVENT` yields `(code, windowID)`.
- Event filter (advanced): `SetEventFilter(callable | None)` / `GetEventFilter`
  (one global filter; callback returns truthy to keep the event) and
  `FilterEvents(callable)` (run once over the queue) — both through the
  `PySDL_ThreadEnter` trampoline.
- Constants: `DROP*`, `FINGER*`, `MULTIGESTURE`, `DOLLAR*`, `CLIPBOARDUPDATE`,
  `SENSORUPDATE`, `RENDER_TARGETS_RESET` / `RENDER_DEVICE_RESET`, `FIRSTEVENT` /
  `LASTEVENT` / `ADDEVENT` / `PEEKEVENT` / `GETEVENT`.

## Phase 6 - Timers, haptics, sensors, touch, power  — DONE

Tests: `tests/test_timer.py`, `tests/test_system.py`; example: `example/timer.py`.

- New **`SDL2.Timer(interval_ms, callback)`** (`src/pysdl_Timer.c`): `Remove()`,
  `id`. Callback fires on SDL's timer thread via the `PySDL_ThreadEnter`
  trampoline; its return value is the next interval (`None` = same, `0`/falsy =
  stop). Dropping the wrapper cancels the timer (GIL dropped around
  `SDL_RemoveTimer`).
- New **`SDL2.Haptic(device_index)`** (`src/pysdl_Haptic.c`): `Query`, `NumAxes`,
  `NumEffects` / `NumEffectsPlaying`, `RumbleSupported` / `RumbleInit` /
  `RumblePlay(strength, ms)` / `RumbleStop`, `SetGain` / `SetAutocenter`,
  `Pause` / `Unpause` / `StopAll`, `EffectSupported` / `NewEffect` /
  `UpdateEffect` / `RunEffect` / `StopEffect` / `DestroyEffect` /
  `GetEffectStatus` (effects passed as a dict with `type` = LEFTRIGHT / CONSTANT
  / SINE / TRIANGLE / SAWTOOTH*). Module: `NumHaptics`, `HapticName`,
  `MouseIsHaptic`, `HapticOpenFromMouse`, `JoystickIsHaptic`,
  `HapticOpenFromJoystick`.
- New **`SDL2.Sensor(device_index)`** (`src/pysdl_Sensor.c`): `GetName`,
  `GetType`, `GetNonPortableType`, `GetInstanceID`, `GetData(count=6)`, `Close`.
  Module: `NumSensors`, `SensorGetDeviceName` / `…Type` / `…InstanceID`,
  `SensorUpdate`.
- Touch (module, in `pysdl_input.c`): `GetNumTouchDevices`, `GetTouchDevice`,
  `GetTouchDeviceType`, `GetTouchName` (≥2.0.22), `GetNumTouchFingers`,
  `GetTouchFinger` -> `(id, x, y, pressure)`, `RecordGesture`,
  `LoadDollarTemplates` / `SaveDollarTemplate` / `SaveAllDollarTemplates` (path).
- Power: `GetPowerInfo` -> `(state, seconds, percent)`. CPU extras: `GetSystemRAM`,
  `HasNEON`, `HasAVX512F`, `HasARMSIMD` (≥2.0.12), `HasLSX` / `HasLASX` (≥2.24),
  `SIMDGetAlignment` (≥2.0.10).
- Constants: `INIT_SENSOR`, `HAPTIC_*`, `SENSOR_*`, `TOUCH_DEVICE_*`,
  `TOUCH_MOUSEID` / `MOUSE_TOUCHID`, `POWERSTATE_*`.

## Phase 7 - Video/window completeness & system integration  — DONE

New `src/pysdl_video.c` for the module-level video functions. Tests:
`tests/test_video.py`; example: `example/window.py`.

- **Window** methods added (`pysdl_Window.c`): `GetWindowFlags`,
  `GetWindowDisplayIndex`, `GetWindowPixelFormat`, `Set/GetWindowMinimumSize`,
  `Set/GetWindowMaximumSize`, `SetWindowBordered`, `SetWindowInputFocus`,
  `SetWindowModalFor`, `Set/GetWindowGrab`, `Set/GetWindowOpacity`,
  `GetWindowBordersSize`, `UpdateWindowSurfaceRects`, `Set/GetWindowGammaRamp`
  (256-value channels), `Set/GetWindowDisplayMode`; `SetWindowAlwaysOnTop`,
  `Set/GetWindowKeyboardGrab`, `Set/GetWindowMouseGrab`, `FlashWindow` (≥2.0.16);
  `Set/GetWindowMouseRect` (≥2.0.18). (`SetWindowHitTest` skipped — a hit-test
  callback per pixel is a poor fit here.)
- **Display** (module): `GetDisplayName`, `GetDisplayUsableBounds`,
  `GetDisplayOrientation`, `GetNumDisplayModes`, `GetClosestDisplayMode`;
  `GetPointDisplayIndex` / `GetRectDisplayIndex` (≥2.24). `GetDisplayMode(display,
  mode=0)` now takes the mode index.
- **Message box**: `ShowSimpleMessageBox(title, message, flags=INFORMATION,
  window=None)`, `ShowMessageBox({title, message, buttons, flags, window})` ->
  selected button id (buttons are `(id, text[, flags])`).
- **Hints**: `SetHint`, `SetHintWithPriority`, `GetHint`, `GetHintBoolean`,
  `ResetHint` (≥2.24), `ClearHints`.
- **Misc**: `OpenURL`, `GetPreferredLocales` -> `[(lang, country_or_None), …]`
  (≥2.0.14).
- **Borrowed windows** (module): `GetWindowFromID`, `GetGrabbedWindow` (≥2.0.16),
  `GL_GetCurrentWindow`.
- **GL loaders** (module): `GL_LoadLibrary`, `GL_UnloadLibrary`,
  `GL_GetProcAddress` -> address int, `GL_GetCurrentContext` -> address int.
- **Vulkan**: module `Vulkan_LoadLibrary` / `Vulkan_UnloadLibrary` /
  `Vulkan_GetInstanceExtensions(window=None)` -> `[str]` /
  `Vulkan_GetVkGetInstanceProcAddr()` -> address int; window
  `Vulkan_GetDrawableSize()` -> `(w, h)` and `Vulkan_CreateSurface(vk_instance:int)`
  -> `vk_surface:int` (handles as ints, for interop with a Python Vulkan binding).
- Constants: `WINDOW_VULKAN` / `WINDOW_METAL` / `WINDOW_*_GRABBED`,
  `WINDOWPOS_UNDEFINED`, `MESSAGEBOX_*`, `HINT_DEFAULT` / `NORMAL` / `OVERRIDE`,
  `ORIENTATION_*`, `FLASH_*` (≥2.0.16).

## Phase 8 - Audio completeness  — DONE

Tests: `tests/test_audio.py` (extended); example: `example/wav.py`.

- Module (`pysdl_Audio.c`, `pysdl_audio_methods`): `GetNumAudioDrivers`,
  `GetAudioDriver`, `GetCurrentAudioDriver`, `AudioInit` / `AudioQuit`,
  `LoadWAV(path | bytes)` -> `((freq, format, channels, samples), bytes)`,
  `MixAudioFormat(dst, src, format, volume=MIX_MAXVOLUME)` -> mixed bytes,
  `GetAudioDeviceSpec(index, iscapture=False)` (≥2.0.16),
  `GetDefaultAudioInfo(iscapture=False)` (≥2.24).
- `Audio` methods: `ClearQueued`, `GetStatus` (-> `AUDIO_STOPPED` / `PLAYING` /
  `PAUSED`). `Queue` now accepts any bytes-like; `Dequeue` returns exactly the
  bytes read (both hardened). `Open()`'s `deviceName` is now optional — omit it
  for the system default device.
- New **`SDL2.AudioStream(src_format, src_channels, src_rate, dst_format,
  dst_channels, dst_rate)`** (`src/pysdl_AudioStream.c`): `Put(data)`,
  `Get(max_len)` -> bytes, `Available()`, `Flush()`, `Clear()`; freed on dealloc.
- Constants: `AUDIO_STOPPED` / `PLAYING` / `PAUSED`, `MIX_MAXVOLUME`.

## Phase 9 - Loose ends  — DONE

Tests: `tests/test_rect.py`; example: `example/rects.py`.

- New `src/pysdl_rect.c` (`pysdl_rect_methods`): `HasIntersection`,
  `IntersectRect` (-> rect or `None`), `UnionRect`, `EnclosePoints(points,
  clip=None)`, `IntersectRectAndLine(rect, (x1,y1,x2,y2))` (-> clipped line or
  `None`), `PointInRect`, `RectEmpty`, `RectEquals`; float variants
  `HasIntersectionF` / `IntersectFRect` / `UnionFRect` / `PointInFRect` /
  `FRectEmpty` / `FRectEquals` (≥2.0.22). (No `SDL2/__init__` shim — the binding
  is a single extension module; the helpers are in C over the same tuple rects.)
- **Metal** (rounding out the GL / Vulkan / Metal trio, ≥2.0.11): window
  `Metal_CreateView()` -> view int, `Metal_GetDrawableSize()`; module
  `Metal_GetLayer(view)` -> `CAMetalLayer` pointer int, `Metal_DestroyView(view)`.
- **Bug fix**: ~14 constants (`WINDOW_ALWAYS_ON_TOP`, `WINDOW_VULKAN`,
  `WINDOW_METAL`, `WINDOW_*_GRABBED`, `BLENDMODE_MUL`, `LOCALECHANGED`,
  `PIXELFORMAT_XRGB8888`, …) were guarded with `#ifdef` on an enum member and so
  never registered; switched to `SDL_VERSION_ATLEAST`.
- Filesystem / RWops: covered by the "accept bytes / take a path" decision
  throughout (`LoadBMP` / `LoadImage` / `LoadWAV`, the `…SurfaceFrom` keepalive,
  dollar-template save/load).

## Phase 10 - Joystick & gamepad completeness  — DONE

Ported from pysdl2's phase 10, plus its phase-15 owned `GetJoystick()`. Tests:
`tests/test_joystick.py` (virtual pads incl. callbacks, touchpad and sensor
input); example: `example/gamepad.py` (device info, bindings, sensors,
touchpad / sensor / battery events, `--virtual` pad reporting rumble / LED /
player; `--demo` drives it unattended, so it runs headless).

- **Joystick** methods: `GetVendor` / `GetProduct` / `GetProductVersion` /
  `GetFirmwareVersion` / `GetType` / `GetSerial` / `Path` (`None` when absent),
  `GetAxisInitialState` (-> value or `None`), `GetPlayerIndex` /
  `SetPlayerIndex`, `SendEffect(bytes)`, `HasRumble` / `HasRumbleTriggers`
  (capability properties, like `HasLED`). SDL3-only: `GetPowerInfo()` ->
  `(POWERSTATE_*, percent)`, `GetConnectionState()` -> `JOYSTICK_CONNECTION_*`,
  `SetVirtualBall`, `SetVirtualTouchpad(touchpad, finger, down, x, y, pressure)`,
  `SendVirtualSensorData(type, values, sensor_timestamp=0)`.
- Joystick module (by instance id; pysdl2 name + SDL3 alias):
  `JoystickGetDevice{GUID,Vendor,Product,ProductVersion,Type,PlayerIndex}` /
  `GetJoystick…ForID`, `JoystickPathForIndex` / `GetJoystickPathForID`,
  `JoystickFromInstanceID` / `GetJoystickFromID`, `JoystickFromPlayerIndex` /
  `GetJoystickFromPlayerIndex`, `LockJoysticks` / `UnlockJoysticks`,
  `UpdateJoysticks` (alias of `JoystickUpdate`).
- **GUIDs** stay 32-char hex strings; anything taking one also accepts the raw 16
  bytes (`PyToGUID` / `GUIDToPy`, declared in `pysdl.h`). A str must be exactly
  32 hex digits (SDL would read junk as zeros). `JoystickGetGUIDFromString` /
  `GUIDFromString` / `StringToGUID` -> 16 bytes, `JoystickGetGUIDString` /
  `GUIDToString` -> str, `GetJoystickGUIDInfo` -> `(vendor, product, version, crc16)`.
- **Virtual joysticks**: `JoystickAttachVirtual` / `JoystickAttachVirtualEx` /
  `AttachVirtualJoystick` are one function: `(type=JOYSTICK_TYPE_GAMEPAD,
  naxes=0, nbuttons=0, nhats=0, *, nballs, vendor_id, product_id, button_mask,
  axis_mask, name, touchpads=[nfingers, …], sensors=[type | (type, rate_hz), …],
  update, set_player_index, rumble, rumble_triggers, set_led, send_effect,
  set_sensors_enabled)` -> instance id. Callbacks go through
  `PySDL_ThreadEnter`; returning `None` / truthy reports success, falsy or an
  exception (printed) reports failure and sets SDL's error. Only the given
  callbacks are wired, so e.g. `HasRumble()` reflects whether `rumble=` was
  passed. SDL only ever gets an integer token as userdata: the callbacks sit in
  a token-keyed dict entry made *before* the attach (SDL calls
  `set_player_index` from inside it) and dropped by SDL's `Cleanup` callback,
  which runs on `JoystickDetachVirtual` / `DetachVirtualJoystick`, subsystem
  quit, or a failed attach. `IsJoystickVirtual` alias.
- **Gamepad** methods: `GetBindForAxis` / `GetBindForButton` (-> `None`,
  `(GAMEPAD_BINDTYPE_BUTTON|AXIS, n)` or `(GAMEPAD_BINDTYPE_HAT, hat, mask)`,
  now built on `SDL_GetGamepadBindings`), `GetVendor` / `GetProduct` /
  `GetProductVersion` / `GetFirmwareVersion` / `GetSerial` / `Path` /
  `GetSteamHandle`, `GetPlayerIndex` / `SetPlayerIndex`, `HasLED` / `HasRumble` /
  `HasRumbleTriggers`, `HasAxis` / `HasButton`, `HasSensor` / `SetSensorEnabled` /
  `IsSensorEnabled` / `GetSensorDataRate` / `GetSensorData(type, count=3)`,
  `GetNumTouchpads` / `GetNumTouchpadFingers` / `GetTouchpadFinger` (->
  `(down, x, y, pressure)`), `SendEffect`, `GetAppleSFSymbolsNameFor{Button,Axis}`
  (`None` off Apple). SDL3-only: `GetBindings()` -> `[(input, output), …]` with
  input `(BINDTYPE_BUTTON, button)` / `(BINDTYPE_AXIS, axis, min, max)` /
  `(BINDTYPE_HAT, hat, mask)` and output `(BINDTYPE_BUTTON, GAMEPAD_BUTTON_*)` /
  `(BINDTYPE_AXIS, GAMEPAD_AXIS_*, min, max)` (`None` for an unbound side);
  `InstanceID`, `GetRealType`, `GetButtonLabel` (-> `GAMEPAD_BUTTON_LABEL_*`),
  `GetPowerInfo`, `GetConnectionState`.
- Gamepad module: `GetGamepads()`, `GameControllerFromInstanceID` /
  `GetGamepadFromID`, `GameControllerFromPlayerIndex` /
  `GetGamepadFromPlayerIndex`, `GameControllerTypeForIndex` /
  `GetGamepadTypeForID`, `GameControllerMappingForDeviceIndex` /
  `GetGamepadMappingForID`, `GameControllerPathForIndex` / `GetGamepadPathForID`,
  `GameControllerMappingForGUID` / `GetGamepadMappingForGUID`,
  `GameControllerNumMappings` / `GameControllerMappingForIndex` (over
  `GetGamepadMappings()` -> `[str]`), `GameControllerGet{Axis,Button}FromString`
  / `GetGamepad{Axis,Button}FromString`, `GameControllerGetStringFor{Axis,Button}`
  / `GetGamepadStringFor{Axis,Button}`, `GetGamepadTypeFromString` /
  `GetGamepadStringForType`. `AddGamepadMappingsFromFile` also takes the file's
  bytes (`SDL_AddGamepadMappingsFromIO`).
- **Owned lookups**: `Gamepad.GetJoystick()` and every `*FromID` /
  `*FromPlayerIndex` return a new owned wrapper that re-opens the device by id
  (SDL refcounts opens), so closing one handle never invalidates another;
  `None` when nothing matching is open. `GetJoystick()` used to return a
  borrowed pointer that dangled once the gamepad closed. `Joystick.shouldFree`
  is gone: nothing borrows a joystick any more.
- `_event()`: `EVENT_GAMEPAD_TOUCHPAD_DOWN/MOTION/UP` -> `(which, touchpad,
  finger, x, y, pressure)`; `EVENT_GAMEPAD_SENSOR_UPDATE` -> `(which, sensor,
  (x, y, z), sensor_timestamp_ns)`; `EVENT_JOYSTICK_BATTERY_UPDATED` ->
  `(which, POWERSTATE_*, percent)`; `EVENT_JOYSTICK_UPDATE_COMPLETE` /
  `EVENT_GAMEPAD_UPDATE_COMPLETE` / `EVENT_GAMEPAD_STEAM_HANDLE_UPDATED` ->
  `(which,)`. The constants were already generated; none added.
- **Bug fix (regression test)**: closing a rumbling joystick stops it through
  the `rumble` callback, which can run from a dealloc while an exception
  unwinds; the callbacks now set the pending exception aside (it was a fatal
  interpreter error).

SDL3 differences and skips:

- No device indices: the `*ForIndex` / `*GetDevice*` names take an instance id.
  `JoystickGetDeviceInstanceID` is dropped (the id *is* the instance id).
- `GetSensorDataWithTimestamp` is gone from SDL3; a reading's timestamp now
  arrives only as the 4th item of `EVENT_GAMEPAD_SENSOR_UPDATE`.
- Power levels became `SDL_PowerState` + percent (`GetPowerInfo`;
  `CurrentPowerLevel` still returns just the state). The battery event changed
  shape the same way.
- `SendVirtualSensorData` raises on SDL < 3.2.22: those releases never grow the
  virtual sensor queue past one slot, so a second sample before the next update
  writes past the heap buffer (fixed upstream in 3.2.22). Its test skips that
  part on Debian 13 (SDL 3.2.10).
- SDL (3.2 – 3.4) never reports a virtual joystick's balls (`NumBalls()` is 0,
  no `JOYSTICK_BALL_MOTION`), so `nballs` / `SetVirtualBall` are accepted but
  inert. `naxes` / `nbuttons` / `nhats` / `nballs` are capped at 255: SDL walks
  them with a `Uint8` index and would loop forever.
- Virtual pads report `GAMEPAD_TYPE_STANDARD` (SDL2 had `CONTROLLER_TYPE_VIRTUAL`).
- The callbacks run with SDL's joystick lock held and need the GIL, so with
  several Python threads keep joystick calls on the thread that pumps events (a
  thread in `WaitEvent` running `update` and another calling a joystick
  function can deadlock). `LockJoysticks` has the same caveat.
- pysdl2's "some cleanup" in that commit: conftest's always-rebuild was already
  covered by the SDL3 conftest's mtime check; `test_audio` gained its `Flush()`;
  the event-filter `POLLSENTINEL` guard does not apply (SDL3 never runs filters
  on `EVENT_POLL_SENTINEL`); the `pysdl.h` comment fixes were applied. Of the CI
  fixes, the `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS` test fixture is ported; the
  sdl2-compat result inversion does not apply to native SDL3.

## Explicitly out of scope

Threads / mutexes / semaphores / condition vars / atomics (use Python's),
`SDL_Log*`, assertions, `SDL_main` / main callbacks, platform-specific APIs
(Android / iOS / WinRT), `GetWindowWMInfo`, stdinc shims. Satellite libraries
(`SDL3_ttf`, `SDL3_mixer`, `SDL3_net`) are a separate effort - each its own
linked lib + `pysdl_ttf.c` etc., or separate packages.

Removed by SDL3 with no in-style replacement (dropped from the binding in the
SDL3 port): gesture recognition (`MULTIGESTURE` / `DOLLAR*`, `RecordGesture`,
dollar templates), window gamma ramp / brightness, `Texture.GL_Bind`,
`GetDisplayDPI`, `IMG_Init` / `IMG_Quit`, `GetRendererInfo` /
`GetRenderDriverInfo` (now name + properties), `RenderTargetSupported`,
`SDL_QueryTexture` (now `Texture.Query` over size + properties),
`RenderSetIntegerScale`, `AudioInit` / `AudioQuit`. The new SDL3 subsystems
(GPU, camera, pen, async I/O, dialogs, tray) are future work.

## Per-phase checklist

Each phase ships: new constants, `.tp_doc` + terse method docstrings matching the
existing style, one `example/` script, one headless `tests/test_<area>.py`,
and a README coverage note.
