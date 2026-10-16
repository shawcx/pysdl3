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
- The callbacks run with SDL's joystick lock held and need the GIL, so a
  thread in `WaitEvent` running `update` and another calling a joystick
  function could deadlock. Fixed in phase 13: joystick / gamepad calls now take
  that lock with the GIL released.
- pysdl2's "some cleanup" in that commit: conftest's always-rebuild was already
  covered by the SDL3 conftest's mtime check; `test_audio` gained its `Flush()`;
  the event-filter `POLLSENTINEL` guard does not apply (SDL3 never runs filters
  on `EVENT_POLL_SENTINEL`); the `pysdl.h` comment fixes were applied. Of the CI
  fixes, the `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS` test fixture is ported; the
  sdl2-compat result inversion does not apply to native SDL3.

## Phase 11 - SDL_image completeness  — DONE

Ported from pysdl2's phase 11, plus its phase-12 `SaveBMP(path=None)` (the
same stream helper does the work). Tests: `tests/test_image.py` (hand-built
GIF / TGA / SVG / XPM fixtures; the SDL_image 3.4 parts skip on 3.2); example:
`example/image.py` (no window, runs headless).

- New `src/pysdl_Image.c` (`pysdl_image_methods`); `LoadImage` moved there
  from `pysdl.c`. Module functions keep the `IMG_` prefix: `IMG_Version()` ->
  `(major, minor, micro)` of the loaded library (`IMG_Linked_Version` alias, the
  pysdl2 name), `IMG_is{AVIF,BMP,CUR,GIF,ICO,JPG,JXL,LBM,PCX,PNG,PNM,QOI,SVG,
  TIF,WEBP,XCF,XPM,XV}(src)` -> bool, `IMG_LoadSizedSVG(src, w, h)`,
  `IMG_ReadXPMFromArray(lines)` / `IMG_ReadXPMFromArrayToRGB888(lines)`.
- `IMG_LoadAnimation(src, type=None)` -> `(w, h, [(Surface, delay_ms), …])`.
  Frames are moved out of the `IMG_Animation` into owned Surfaces before
  `IMG_FreeAnimation`, so there is no animation type to manage; a still image
  comes back as one frame.
- `LoadImage(src, type=None)` and `Renderer.LoadTexture(src, type=None)` take a
  path, os.PathLike or any bytes-like. SDL_image detects every format with a
  magic number from the data, so `type` only matters for magic-less TGA (a path
  with no type still goes through `IMG_Load(file)`, whose extension names it);
  that makes the per-format `IMG_Load<FMT>_IO` / `IMG_Load<FMT>Animation_IO`
  equivalent and they are not exposed separately. `LoadBMP` takes the same
  sources (it gained os.PathLike and memoryview).
- `Surface.SavePNG(path=None)` / `SaveJPG(path=None, quality=90)` /
  `SaveBMP(path=None)` and the new `SaveAVIF(path=None, quality=90)`: with no
  path they return the encoded `bytes`, written through
  `SDL_IOFromDynamicMem` (`PySDL_SaveOpen` / `PySDL_SaveClose` in
  `pysdl_util.c`); paths may be os.PathLike. `SaveBMP` stays `SDL_SaveBMP_IO`
  (core SDL), not SDL_image 3.4's `IMG_SaveBMP`.
- Shared `PySDL_IOFromObject` (path or buffer -> `SDL_IOStream`) backs every
  loader and probe; an empty buffer becomes an empty stream (SDL 3.2's
  `SDL_IOFromConstMem` rejects size 0), so probes say `False` and loaders raise.
- SDL_image 3.4 (`SDL_IMAGE_VERSION_ATLEAST(3,4,0)`, tests skip without):
  `Surface.SaveGIF` / `SaveTGA` / `SaveICO` / `SaveCUR(path=None)`,
  `SaveWEBP(path=None, quality=90.0)` (a float, as in SDL), `Save(path=None,
  type=None)` (`IMG_SaveTyped_IO`; the type defaults to the path's extension,
  as `IMG_Save` does); `IMG_isANI`; `IMG_SaveAnimation(frames, path=None,
  type=None)` -> `None` or bytes (GIF / WEBP / APNG / AVIF / ANI, the inverse of
  `IMG_LoadAnimation`: frames must share one size and have delays >= 0; the
  surfaces are borrowed for the call, GIL held); `IMG_GetClipboardImage()` ->
  Surface or `None`.

SDL3 differences, fixes and skips:

- SDL3_image has no `IMG_Init` / `IMG_Quit` / `IMG_INIT_*` (dropped in the
  port, still absent); `IMG_Linked_Version`'s `SDL_version *` became
  `IMG_Version()`'s packed int, unpacked into the same tuple.
- `*_RW` -> `*_IO`. SDL_image reads everything during a load call (and closes
  the stream when asked), so a bytes source is pinned only for the call; no
  object exposed here keeps an `SDL_IOStream`. `IMG_LoadSizedSVG_IO` and the
  probes take no `closeio`; the binding closes those streams itself.
- GIL: dropped for loads, probes and file opens / closes, as before. Saves keep
  it while encoding (the encoder reads a Surface another thread could free,
  e.g. a window surface invalidated by a resize).
- **Bug fix (regression tests)**: SDL_image reads an XPM array without bounds
  checks — a colour line shorter than `chars_per_pixel + 1` or a pixel row
  shorter than `width * chars_per_pixel` is read past its end (confirmed under
  valgrind against Debian's 3.2.4), and before 3.4.4 a one-char pixel with no
  colour dereferences NULL (Debian's 3.2.4 carries that fix as a patch). The
  binding now parses the header strictly and checks the line count, lengths and
  one-char keys first (`ValueError`). pysdl2's port has the same hole.
- Saving an indexed surface with no palette raises on every version (SDL_image
  3.4 refuses; 3.2's `IMG_SavePNG_IO` reported success having written nothing).
  SDL 3.2+ `CreateSurface` gives indexed surfaces no palette until `SetPalette`. Saving an uninitialised
  Surface raises instead of passing SDL a NULL.
- Left for later: the SDL_image 3.4 streaming `IMG_AnimationEncoder` /
  `IMG_AnimationDecoder` objects (they would need wrapper types, and a decoder
  keeps its `SDL_IOStream` — and so a pinned buffer — open across calls);
  `IMG_CreateAnimatedCursor` (cursor lifetime vs. its frames needs checking
  against SDL 3.4's `SDL_CreateAnimatedCursor`); `IMG_LoadGPUTexture*` (no GPU
  binding); metadata properties (`IMG_PROP_METADATA_*`).

## Phase 12 - Render, surface & window completeness  — DONE

Ported from pysdl2's phase 12, together with the renderer -> texture
invalidation of its phase 15 (the cursor part came with phase 13).
Tests: phase-12 sections of `tests/test_renderer.py`, `test_texture.py`,
`test_surface.py`, `test_video.py` (the `Quit` cases run in a subprocess; the
parent-window case needs a real video driver and skips under dummy); example:
`example/logical.py` (`--frames N` exits after N frames, so it runs headless).

- **Renderer**: `RenderWindowToLogical((x, y))` / `RenderCoordinatesFromWindow`
  and `RenderLogicalToWindow` / `RenderCoordinatesToWindow` (floats both ways
  in SDL3), `RenderGetWindow` / `GetRenderWindow` (a borrowed Window, `None` for
  a software renderer on a Surface), `RenderGetMetalLayer` /
  `GetRenderMetalLayer` and `RenderGetMetalCommandEncoder` /
  `GetRenderMetalCommandEncoder` (pointer int, `None` off Metal), `GetVSync`,
  `RenderSetVSync` (pysdl2's name for the existing `SetVSync`),
  `GetRenderLogicalPresentationRect` (where the logical canvas lands, floats),
  and `Set/GetRenderLogicalPresentation` as SDL3-name aliases of
  `RenderSet/GetLogicalSize`.
- **`RenderGeometryRaw(texture, xy, color, uv=None, indices=None, *,
  num_vertices=None, xy_stride=8, color_stride=16, uv_stride=8, index_size=4)`**:
  triangles straight from any buffer (bytes, `array`, numpy, memoryview slices
  for interleaved records). `xy` / `uv` are float32 pairs, `color` is float32
  RGBA (`SDL_FColor`; `color_stride=0` gives every vertex one colour), indices
  1/2/4-byte unsigned. `num_vertices` defaults to `len(xy) // xy_stride`.
  Buffer sizes, non-negative strides and float / index alignment are checked
  before SDL reads anything; SDL range-checks the index values itself.
- Module: `CreateWindowAndRenderer(title, size, flags=0)` -> `(Window,
  Renderer)`, both owned.
- **Texture**: `LockToSurface(rect=None)` -> a Surface over the locked region
  (the rect clipped to the texture, as SDL does); `Unlock` (and the texture
  going away) swaps it for an empty 0x0 surface so it never dangles.
  `UpdateNV(yplane, ypitch, uvplane, uvpitch, rect=None)`, with the planes and
  pitches checked against the clipped update area like `UpdateYUV`.
- **Surface**: `HasColorKey`, `HasRLE`, `SoftStretchLinear(src, srcrect=None,
  dstrect=None)` and `Stretch(..., scale_mode=SCALEMODE_NEAREST)` (both
  `SDL_StretchSurface`). The `w` / `h` / `pitch` / `format` / `pixels` / `bpp`
  getters (and the `pixels` setter) raise on an uninitialised Surface instead
  of dereferencing NULL. (`SaveBMP(path=None)` -> bytes came with phase 11.)
- **Pixels** (module): `ConvertPixels(size, src_format, src, dst_format,
  src_pitch=0, dst_pitch=0)` and `PremultiplyAlpha(..., linear=False)` ->
  bytes. Pitch 0 = tight rows; YUV sources and destinations are sized the way
  SDL lays them out (chroma planes after the luma rows; shared helper
  `PySDL_PixelsSize`), pitches shorter than a row are refused, and `src` must
  cover the whole image before SDL reads it.
- **Window**: `GetRenderer()` (the owning Renderer object itself, or `None`),
  `GetWindowICCProfile()` -> bytes or `None` (SDL's copy freed),
  `HasWindowSurface` / `WindowHasSurface`, `DestroyWindowSurface` (empties the
  `GetWindowSurface()` Surface first; refused while a Renderer draws to the
  window), `SetWindowShape(surface | None)`. `GetWindowSizeInPixels` was
  already there.
- Constants: none needed (`LOGICAL_PRESENTATION_*`, `RENDERER_VSYNC_*`,
  `SCALEMODE_*`, `WINDOW_TRANSPARENT` were already generated).

**Renderer / texture lifetimes** (pysdl2 phase 15's renderer part, reworked for
SDL3, where more frees renderers behind the binding's back):

- One wrapper per SDL renderer. `Window.GetRenderer()` returns the owning
  `Renderer` rather than a borrowed wrapper, so `Renderer` needs no
  `shouldFree` (pysdl2 added one for `GetRenderer`) and identity holds
  (`window.GetRenderer() is renderer`). SDL's own renderer behind an
  accelerated window surface is not exposed.
- Texture wrappers record their renderer (`PySDL_TextureTrack` at the three
  creation sites). Whenever SDL frees a renderer's textures the wrappers are
  invalidated first (methods raise, a `LockToSurface` Surface is emptied):
  when the Renderer is dropped; when its Window is — `SDL_DestroyWindow`
  destroys the window's renderer but keeps a husk until `SDL_DestroyRenderer`,
  and `SDL_FlushRenderer` doesn't check for that, so the Window destroys its
  Renderer first and nulls it (methods raise "Invalid renderer"); and on video
  shutdown (`Quit`, a `QuitSubSystem` that stops video), whose `SDL_QuitRender`
  frees every renderer, software ones included. `SetRenderTarget` /
  `RenderGeometry` / `RenderGeometryRaw` / `Copy*` raise on an invalidated
  texture instead of reading it as "no texture".
- `SDL_DestroyWindow` also destroys the window's **child windows**
  (`SetWindowModalFor` parents them); the Window's dealloc now nulls their
  wrappers too and invalidates their surfaces, renderers and textures (it only
  handled the window itself).
- `CreateSoftwareRenderer(surface)` keeps the Surface alive (SDL holds the
  pointer: pysdl2's own tests drew into a freed temporary) and refuses
  SDL-owned surfaces (a window surface, a `LockToSurface` surface) that SDL
  could free under it. A software renderer on a window draws into, and on
  destruction frees, the window surface: the `GetWindowSurface()` Surface is
  emptied first.
- `QuitSubSystem` only invalidates when video was running before the call: a
  software renderer on a Surface needs no video and survives other subsystems
  stopping.

Bug fixes (regression tests):

- `Texture.Lock(rect)`: SDL doesn't clip the rect, so an off-texture rect gave
  a *writable* view outside the pixels, and the view (`pitch * h` bytes from
  the rect's first pixel) ran past the buffer for any rect touching the bottom
  edge. The rect must now lie within the texture (`ValueError`), and the view
  ends with the last row's `w` pixels: **behaviour change**, its length is
  `pitch * (h - 1) + w * bytes_per_pixel` (was `pitch * h`).
- `Texture.Update` handed SDL the buffer unchecked; it must now cover the
  clipped update (YUV chroma planes included).
- `Texture.UpdateYUV` with a rect off the texture in one direction only
  crashed (SDL copies rows of negative width when the clip is empty that way);
  a rect that misses the texture is now a no-op, as in `SDL_UpdateTexture`.
- `Surface.SoftStretch` with a 2-item rect (`w = h = -1`) crashed, and a
  zero-width source rect read past the row: `SDL_StretchSurface` trusts the
  sizes. Negative sizes now raise, zero sizes copy nothing, and a YUV
  destination (whose dstrect SDL writes unclipped) is stretched to whole.
- `Renderer.RenderFlush()` / `SetRenderTarget()` on an uninitialised or
  invalidated Renderer crashed (`SDL_FlushRenderer`, and SDL 3.2's
  `SDL_SetRenderTarget`, dereference it unchecked); they raise now.

SDL3 differences and skips:

- `RenderGeometryRaw` colours are `SDL_FColor` floats (pysdl2: RGBA bytes,
  default stride 4); window <-> render coordinates are floats both ways
  (pysdl2's `RenderLogicalToWindow` returned ints); `CreateWindowAndRenderer`
  takes a title.
- Shaped windows: SDL3 dropped `CreateShapedWindow` / `IsShapedWindow` /
  `GetShapedWindowMode` / `SHAPEMODE_*` for a per-window alpha shape on
  `WINDOW_TRANSPARENT` windows (`SetWindowShape`). Removing a shape (`None`)
  only works with SDL 3.4.4+ (earlier SDL rejects NULL); the binding checks the
  linked version and says so.
- Gone in SDL3, skipped: `VideoInit` / `VideoQuit` (use `InitSubSystem` /
  `QuitSubSystem(INIT_VIDEO)`), `CalculateGammaRamp` and
  `Get/SetWindowBrightness` (gamma ramps were removed), `Set/GetYUVConversionMode`
  / `GetYUVConversionModeForResolution` and `YUV_CONVERSION_*` (replaced by
  colorspaces).
- `CreateWindowFrom` skipped: SDL3 replaced it with
  `SDL_CreateWindowWithProperties` plus a native-handle property that differs
  per video driver (Cocoa `NSWindow*`, Win32 `HWND`, X11 `Window`, Wayland
  `wl_surface*`), so it isn't portable, and no driver the tests run under can
  exercise it. A generic `CreateWindowWithProperties` is the right shape later.
- `SDL_ConvertEventToRenderCoordinates` not wrapped: events are tuples here;
  map their coordinates with `RenderWindowToLogical`.
- `ConvertPixels` to / from `PIXELFORMAT_P010` needs a colorspace conversion
  SDL doesn't do by default ("Unsupported YUV colorspace"); its sizes are
  validated all the same.
- Still skipped on purpose, as in pysdl2: `SDL_BlitSurfaceUnchecked*` (the old
  `LowerBlit`: unclipped, writes out of bounds on a bad rect; `Blit` /
  `BlitScaled` cover it), and the window / texture property bags that replaced
  `SetWindowData` / texture user data (keep a dict keyed by the wrapper or the
  window id).
- Known remaining: `Texture.Lock()`'s memoryview is a raw view of SDL's
  memory and still dangles if kept past `Unlock()` or the texture's renderer
  (as in pysdl2).

## Phase 13 - Loose ends II  — DONE

Ported from pysdl2's phase 13, together with the cursor half of its phase 15,
which completes the phase-15 port (windows and window surfaces predate the
SDL3 port, joysticks came with phase 10, renderers and textures with phase 12).
Tests: phase-13 sections of `tests/test_events.py` (watchers, the GIL rule;
the cross-thread and `Quit` cases run in a subprocess that is killed on a
deadlock), `test_video.py` (hint callbacks), `test_cursor.py` (cursor
lifetimes), `test_joystick.py` (the joystick lock), `test_timer.py` (`Quit`
while callbacks run), `test_rect.py`,
`test_input.py`, `test_system.py`; example: `example/events.py` (watcher + hint
callback; `--seconds N` quits by itself, so it runs headless).

- **Event watchers**: `AddEventWatch(callback)` / `RemoveEventWatch(callback)`
  (`DelEventWatch`, pysdl2's name). Watchers run after the filter, only for
  events it keeps, on whichever thread pushed the event; the return value is
  ignored. Adding a callback twice registers it twice; removing drops the
  latest registration (by identity, `ValueError` if none). A watcher may remove
  itself or another one (SDL skips it for the rest of that dispatch). SDL is
  handed an integer token, never the callable (the Timer design): the
  trampoline looks it up under the GIL, and removal deletes the entry first.
- **GIL rule for the event queue**: SDL runs the filter and watchers under its
  event-watcher lock (and `SDL_SetEventFilter` / `SDL_FilterEvents` run a
  filter holding the queue lock too); the trampoline then takes the GIL, which
  the Python callback can hand to another thread mid-call. So `PumpEvents`,
  `PollEvent`, `PushEvent`, `FlushEvent(s)`, `HasEvent(s)`, `EventState` /
  `SetEventEnabled`, `QuitRequested`, `SetEventFilter`, `FilterEvents`,
  `Add/RemoveEventWatch`, `ResetKeyboard` and the clipboard setters (which push
  `EVENT_CLIPBOARD_UPDATE`) now drop the GIL, as `WaitEvent(Timeout)` /
  `PeepEvents` already did. **Bug fix (regression test)**: with a filter
  installed, two threads pushing events deadlocked within milliseconds.
- **Bug fix (regression test)**: the filter trampoline read the current filter
  *before* taking the GIL, so replacing the filter could free one a call on
  another thread was about to use. It now reads it under the GIL (SDL still
  gets no Python pointer). Filter / watcher trampolines also set aside an
  exception that is already pending: destroying a window from a dealloc while
  an exception unwinds pushes `EVENT_WINDOW_DESTROYED`.
- Stopping the event subsystem (`Quit`, or `QuitSubSystem` dropping its last
  reference) makes SDL forget the filter and watchers; the binding forgets them
  too (`GetEventFilter()` -> `None`, `RemoveEventWatch` -> `ValueError`).
- **Hint callbacks**: `AddHintCallback(name, callback)` -> `callback(name, old,
  new)`, called once right away (`old == new`) and then on every change of that
  hint (`None` when unset), on the thread that changed it; adding the same
  callback again only repeats the first call. `RemoveHintCallback`
  (`DelHintCallback`), `ResetHints`. SDL gets one callback per watched hint
  name (userdata `NULL`), added with the first Python callback and left in
  place; the callables live in a name -> list dict read under the GIL. SDL
  walks a hint's callback list saving only the next node, so a callback that
  removed a *later* SDL registration (pysdl2 registered one per callable) would
  make SDL visit freed memory. SDL runs hint callbacks under its hint lock, so
  `Set/GetHint`, `SetHintWithPriority`, `GetHintBoolean`, `ResetHint(s)` and
  `AddHintCallback` drop the GIL. `SDL_Quit` frees every hint callback; `Quit`
  empties the registrations.
- Clipboard: `Get/Set/HasPrimarySelectionText`. Where the video driver has no
  primary selection (dummy, Cocoa) SDL keeps the text itself, so it
  round-trips.
- Keyboard: `ClearComposition(window)` (per-window in SDL3), `ResetKeyboard`;
  SDL3-name aliases `TextInputActive` / `ScreenKeyboardShown` (the pysdl2-era
  `IsTextInputActive` / `IsScreenKeyboardShown` were already there).
- Rect: `EncloseFPoints(points, clip=None)`, `IntersectFRectAndLine(frect,
  line)`. SDL3's float versions have **no** integer `+1` edges (SDL2's kept
  them): the enclosing rect spans exactly min..max and a rect's far edge
  `x + w` counts as inside. **Bug fix**: `IntersectRectAndLine` with a list
  for the line raised `SystemError` (lines may now be tuples or lists).
- Sensors: `SensorFromInstanceID` / `GetSensorFromID` (a new owned Sensor
  re-opened by id, SDL refcounts; `None` if not open),
  `SensorGetDeviceNonPortableType` / `GetSensorNonPortableTypeForID` (-1 for an
  unknown id), `GetSensorTypeForID` alias. Haptic: `HapticOpened(id)`,
  `GetHapticFromID(id)` (owned re-open; `None` if not open).
- Misc: `GetErrorMsg`, an alias of `GetError` (SDL3's error is per-thread
  already and has no `SDL_GetErrorMsg`).

**Cursor lifetimes** (pysdl2 phase 15's cursor part). SDL3 keeps every cursor
in a list and frees them all when video shuts down (`SDL_QuitMouse`, ours and
its default cursor included); `SDL_DestroyCursor` / `SDL_SetCursor` only
compare a pointer against that list, so a stale pointer is not a crash there
but acts on whatever cursor later reuses the address.

- Cursor wrappers register in `tp_init` (`PySDL_Registry`). An owning Cursor's
  dealloc nulls other wrappers of its cursor; video teardown
  (`PySDL_InvalidateWindows` -> `PySDL_InvalidateCursors`) nulls them all, so
  owned ones are not freed a second time. `GetCursor()` returns the owning
  `Cursor` object for our own cursors (only SDL's default cursor is ever
  borrowed). `SetCursor` / `Cursor.Set` raise on an invalid cursor instead of
  passing NULL (SDL's "redraw"), and now raise SDL's own errors instead of
  ignoring them.
- Creating a cursor while video is down raises: SDL would create it through a
  driver that has shut down, or lose it at the next video init.
  `CreateColorCursor` takes its Surface through `PySDL_SurfaceArg`.
- SDL3's dummy driver makes colour / bitmap cursors and a 1x1 default cursor
  without driver support, so these tests run headless (pysdl2's skipped).

**The joystick lock and the GIL** (phase 10's documented deadlock, fixed).
SDL runs virtual-joystick callbacks - so Python - holding its joystick lock,
and with the event calls dropping the GIL a thread pumping events runs
`update` while another calls joystick functions. Every Python entry point of
`pysdl_Joystick.c` / `pysdl_Gamepad.c` (and the two joystick calls in
`pysdl_Haptic.c`) now takes that lock first with the GIL released
(`PySDL_JoystickLock`, a no-op while the joystick subsystem is down) and runs
holding both, so no thread waits for the joystick lock while holding the GIL,
and strings SDL returns are copied before another thread can close the
device. The functions are wrapped by `PYSDL_JOYSTICK_LOCKED(_KW)` (pysdl.h:
`name` calls `name##_impl` under the lock). **Bug fix (regression test)**: a
pumping thread with an `update` callback and a thread calling joystick /
gamepad functions deadlocked within milliseconds. Cost: about 60 ns per call
single-threaded (a GIL release / re-acquire).

**Init / Quit drop the GIL** too. **Bug fix (regression tests in
`tests/test_timer.py`)**: shutting a subsystem down waits for SDL threads that
may be inside a Python callback waiting for the GIL, so holding it deadlocked:
`Quit` -> `SDL_QuitTimers` joining the timer thread while a timer callback
(one `Timer.Remove()` doesn't wait for) waited for the GIL - this is what made
the cross-thread events test hang about once in 35 runs - and
`QuitSubSystem(INIT_AUDIO)` closing a device whose thread held it inside the
playback callback. `Init` / `InitSubSystem` take SDL's joystick lock (the
gamepad subsystem) the same way. The binding's bookkeeping (what was running,
invalidating wrappers, dropping filter / watcher / hint registrations) happens
with the GIL held, before and after; other threads must not use SDL while
these run.

**Timer callbacks are held off during `Quit` / `QuitSubSystem`.** **Bug fix
(regression test)**: `SDL_Quit` tears the event subsystem (and the rest) down
before it stops the timer thread, and `Remove()` doesn't wait for a callback
under way, so a timer callback inside `PushEvent` could block forever on the
event-watcher mutex `SDL_Quit` had just destroyed while `SDL_QuitTimers`
waited to join it - the Linux-only hang of the cross-thread events test (2 of
4 full-suite runs in Debian). `PySDL_TimersHold()` (pysdl_Timer.c) brackets both
calls: callbacks that fire meanwhile skip their Python call (returning the
same interval), and the hold first waits, with the GIL released, for one
already running.

SDL3 differences and skips:

- SDL3 has no `SDL_ClearHints`: `ClearHints` is `ResetHints` (values back to
  their defaults, callbacks kept and called); `SDL_Quit` is what drops the
  callbacks. pysdl2's `ClearHints` dropped them.
- Gone from SDL3, skipped: `SDL_IsTextInputShown`, `SDL_HasRDTSC`,
  `SDL_LockSensors` / `SDL_UnlockSensors` (internal now), and
  `SDL_SensorGetDataWithTimestamp` (a reading's timestamp is only in
  `SDL_SensorEvent.sensor_timestamp`, which `_event()` does not expose for
  `EVENT_SENSOR_UPDATE` yet; gamepad sensor events carry theirs).
- Haptics and sensors are looked up by id, not index (`HapticOpened(id)`).
- SDL3 never runs filters or watchers on its poll sentinel, so pysdl2's
  `POLLSENTINEL` caveats do not apply.
- Residual GIL risks, as in pysdl2: SDL functions that push events only as a
  side effect (most window calls) still hold the GIL, which needs a filter /
  watcher plus a concurrent push from another thread to bite. Hint callbacks
  run under SDL's hint lock and almost every SDL call reads hints, so a hint
  callback that runs long enough to be preempted can deadlock against SDL
  calls on another thread: keep hint callbacks short, and change hints from
  one thread.
- Found, not fixed (predates this phase): `SDL_Quit` frees SDL's cached
  `SDL_PixelFormatDetails`, which every `PixelFormat` wrapper and every Surface
  created before it still point at; using them after `Quit` reads freed memory
  (seen under valgrind). An `Audio` kept past `QuitSubSystem(INIT_AUDIO)` /
  `Quit` crashes in its dealloc (`SDL_DestroyAudioStream` on a stream SDL
  already freed), and `Joystick` / `Gamepad` wrappers are not invalidated when
  the joystick subsystem stops.

## Phase 14 - SDL3_ttf (optional)  — DONE

Tests: `tests/test_ttf.py` (skips without SDL3_ttf or a system font; the
Debian package build-depends on `fonts-dejavu-core` so it runs there);
example: `example/text.py`. Port of pysdl2 `c510c83`.

- **Optional dependency**: `setup.py`'s `OPTIONAL` table (pkg-config name,
  library, define, opt-out env var) probes `pkg-config sdl3-ttf`, else the
  header `SDL3_ttf/SDL_ttf.h`; when found it links `SDL3_ttf` and defines
  `PYSDL_HAVE_TTF`. `PYSDL_TTF=0` builds without it; then `SDL3.Font` /
  `TTF_*` are simply absent. CI's macOS job installs `sdl3_ttf`, and the Debian
  package build-depends on `libsdl3-ttf-dev` (Debian 13 ships 3.2.2).
- New `src/pysdl_Font.c`. Module (keeps the `TTF_` prefix like `IMG_`):
  `TTF_Init`, `TTF_Quit`, `TTF_WasInit` (init is counted), `TTF_Version` ->
  `(major, minor, micro)` (pysdl2's `TTF_Linked_Version` is an alias),
  `TTF_GetFreeTypeVersion` (only known after `TTF_Init`; `(0, 0, 0)` before),
  `TTF_GetHarfBuzzVersion` (`(0, 0, 0)` without HarfBuzz); SDL3:
  `TTF_StringToTag` / `TTF_TagToString`, `TTF_GetGlyphScript(ch)` -> ISO 15924
  tag str.
- **`SDL3.Font(src, ptsize=12.0, index=0, *, hdpi=0, vdpi=0)`**: `src` is a
  path, os.PathLike or the font file's bytes (pinned for the font's lifetime),
  opened through `TTF_OpenFontWithProperties` (stream + autoclose, size, face,
  DPI); point sizes are floats in SDL3. Methods drop `TTF_` and `Font`, pysdl2
  names first with SDL3 aliases: `Close`, `Get/SetStyle`, `Get/SetOutline`,
  `Get/SetHinting`, `Get/SetKerning`, `Height`, `Ascent`, `Descent`, `LineSkip`
  (+ SDL3 `SetLineSkip`), `Faces` (`GetNumFaces`), `FaceIsFixedWidth`
  (`IsFixedWidth`), `IsScalable`, `FaceFamilyName` / `FaceStyleName`
  (`GetFamilyName` / `GetStyleName`), `GetWeight` (SDL3_ttf ≥ 3.2.2),
  `GlyphIsProvided` (`HasGlyph`), `GlyphMetrics(ch)` -> `(minx, maxx, miny,
  maxy, advance)`, `GetKerningSize(a, b)` (`GetGlyphKerning`), `Size(text)`
  (`GetStringSize`), SDL3 `SizeWrapped(text, wrap_width)`, `Measure(text,
  width)` -> `(extent, count)` (`MeasureString`; SDL counts UTF-8 bytes, the
  binding returns characters), `GetSize` / `SetSize` / `SetSizeDPI` /
  `GetDPI`, `Get/SetSDF`, `Get/SetWrappedAlign` (`Get/SetWrapAlignment`),
  `Get/SetDirection`, `GetScript` / `SetScript(tag)` (pysdl2's
  `SetScriptName`; a 4-char str or its int, 0 = automatic), `SetLanguage(bcp47
  or None)`.
- Rendering -> owned `Surface`: `RenderSolid(text, fg, wrap_length=None)`,
  `RenderShaded(text, fg, bg, wrap_length=None)`, `RenderBlended(text, fg,
  wrap_length=None)`, `RenderLCD(text, fg, bg, wrap_length=None)`; a
  `wrap_length` selects the `*_Wrapped` variant (0 = newlines only).
  `RenderGlyph{Solid,Shaded,Blended,LCD}(ch, fg[, bg])`; `ch` is a 1-char str or
  an int codepoint (≤ U+10FFFF). All SDL3_ttf calls are the UTF-8 / 32-bit ones,
  so the pysdl2 version guards are gone.
- Text is a Python `str` passed as UTF-8 with its length; NUL characters raise
  `ValueError` (SDL reads a length of 0 as "NUL-terminated").
- **Lifetime**: a font used after the final `TTF_Quit` raises instead of
  touching freed FreeType state, and is never passed to `TTF_CloseFont` (the
  session counter in `pysdl_Font.c`). The GIL stays held: a `TTF_Font` is not
  thread-safe. Valgrind (Debian, SDL3_ttf 3.2.2) is clean over bytes-backed
  fonts, failed opens, every render / measure path and the post-`TTF_Quit`
  paths.
- Constants: `TTF_STYLE_*`, `TTF_HINTING_*` (incl. `LIGHT_SUBPIXEL`),
  `TTF_HORIZONTAL_ALIGN_*` plus pysdl2's `TTF_WRAPPED_ALIGN_*` aliases,
  `TTF_DIRECTION_*`, `TTF_FONT_WEIGHT_*` (≥ 3.2.2).
- **Not wrapped yet** (SDL3_ttf-only, no pysdl2 counterpart): the text-engine
  API (`TTF_Text`, surface / renderer / GPU text engines), fallback fonts
  (`TTF_AddFallbackFont` - the fallback must outlive every font using it, which
  needs ownership tracking), `TTF_CopyFont`, `TTF_GetGlyphImage(ForIndex)`, the
  outline line-cap / join properties.

## Phase 16 - SDL3_mixer (optional)  — DONE

Not a port of pysdl2's phase 16: SDL3_mixer replaced SDL2_mixer's chunks,
channels and music with mixers, tracks, groups and tags, so this binds the new
API (3.2.0+; 3.2.4 tested) in the binding's style. Tests: `tests/test_mixer.py`
(offline mixers checked sample by sample, device mixers on the dummy driver's
real audio thread, subprocess stress / teardown cases; skips without
SDL3_mixer); example: `example/mixer.py` (`--seconds N`, or `--offline out.wav`).

- **Optional dependency**: `setup.py`'s `OPTIONAL` table (shared with
  SDL3_ttf) links `SDL3_mixer` and defines `PYSDL_HAVE_MIXER` when pkg-config
  `sdl3-mixer` (else `SDL3_mixer/SDL_mixer.h`) is found; `PYSDL_MIXER=0` opts
  out. Debian 13 has no SDL3_mixer (forky / sid do), so the Debian build and
  package go without it; macOS CI installs it. New `src/pysdl_Mixer.c`, wholly
  `#ifdef PYSDL_HAVE_MIXER`.
- Module (prefix kept, like `IMG_`): `MIX_Version()` -> `(3, 2, 4)`,
  `MIX_Init` / `MIX_Quit` (counted: the final `MIX_Quit` invalidates every
  wrapper), `MIX_GetNumAudioDecoders` / `MIX_GetAudioDecoder(i)` /
  `MIX_GetAudioDecoders()`, `MIX_MSToFrames` / `MIX_FramesToMS`,
  `MIX_LoadRawAudio(data, spec, mixer=None)` (copied),
  `MIX_CreateSineWaveAudio(hz, amplitude, ms=-1, mixer=None)`; constants
  `MIX_DURATION_UNKNOWN` / `MIX_DURATION_INFINITE`, `SDL_MIXER_VERSION`.
  Nothing works before `MIX_Init()` (raises). `SDL3.Quit()` first matches every
  outstanding `MIX_Init`, since `SDL_Quit` would stop audio under SDL_mixer.
- **`SDL3.Mixer(spec=None, device=AUDIO_DEVICE_DEFAULT_PLAYBACK)`**
  (`MIX_CreateMixerDevice`); `device=None` with a spec is an offline
  `MIX_CreateMixer` driven by `Generate(nbytes)` -> `(data, mixed_bytes)`.
  `Destroy`, `GetFormat`, `GetDevice`, `Lock` / `Unlock` (same thread, paired;
  for atomic multi-track changes), `CreateTrack`, `CreateGroup`,
  `LoadAudio(src, ...)`, `PlayAudio(audio)` (fire and forget),
  `PlayTag(tag, **options)`, `StopAllTracks` / `StopTag(tag, fade_out_ms=0)`,
  `Pause/ResumeAllTracks`, `Pause/ResumeTag`, `Set/GetGain`, `SetTagGain`,
  `Set/GetFrequencyRatio`, `GetTaggedTracks(tag)` (wrapped tracks only),
  `SetPostMixCallback(fn)`.
- **`SDL3.MixerAudio(src, mixer=None, predecode=False, *, decoder=None,
  skip_metadata=False, ignore_loops=False)`** (`MIX_LoadAudioWithProperties`;
  `SDL3.Audio` is the device wrapper): `src` is a path, os.PathLike or bytes,
  pinned only for the call (SDL_mixer caches it all). `ignore_loops` needs
  3.2.4. `Destroy` (refcounted by SDL: tracks keep playing it),
  `GetDuration`, `GetFormat`, `GetMetadata()` -> dict (title / artist / album /
  copyright / track / total_tracks / year / duration_frames /
  duration_infinite, as present), `MSToFrames` / `FramesToMS`.
- **`SDL3.MixerTrack(mixer)`** / `Mixer.CreateTrack()`: inputs `SetAudio`,
  `SetAudioStream(SDL3.AudioStream)` (kept alive while assigned),
  `SetIOStream(src)` / `SetRawIOStream(src, spec)` (a path, or bytes copied into
  a stream the mixer owns), `GetAudio` / `GetAudioStream`; `Play(**options)`
  (`loops`, `max_frame` / `max_ms`, `start_frame` / `start_ms`,
  `loop_start_frame` / `loop_start_ms`, `fade_in_frames` / `fade_in_ms`,
  `fade_in_start_gain`, `append_silence_frames` / `append_silence_ms`,
  `halt_when_exhausted`, `start_order` with 3.2.2), `Stop(fade_out_frames=0, *,
  fade_out_ms=None)`, `Pause` / `Resume` / `Playing` / `Paused`, tags
  (`Tag` / `Untag` / `GetTags`), position / loops / remaining / fade frames,
  `MSToFrames` / `FramesToMS`, `Set/GetGain`, `Set/GetFrequencyRatio`,
  `SetOutputChannelMap(seq | None)`, `SetStereo((l, r) | None)`,
  `Set3DPosition((x, y, z) | None)` / `Get3DPosition`, `SetGroup`,
  `GetMixer`, `Destroy`; callbacks `SetStoppedCallback(fn(track))`,
  `SetRawCallback` / `SetCookedCallback(fn(track, spec, pcm))`.
- **`SDL3.MixerGroup(mixer)`** / `Mixer.CreateGroup()`: `Destroy`, `GetMixer`,
  `SetPostMixCallback(fn(group, spec, pcm))`.
- **`SDL3.MixerDecoder(src, *, decoder=None)`** (`MIX_CreateAudioDecoder_IO`):
  `GetFormat`, `Decode(nbytes, spec=None)` -> bytes (`b''` at the end, GIL
  released), `Destroy`.
- **Threading**: the mixing thread (device audio thread, or `Generate`'s
  caller) holds the mixer mutex while running callbacks, which then take the
  GIL; every method takes that (recursive) mutex with the GIL released first,
  so the order is the same everywhere and callbacks can call back in.
  Verified by a subprocess stress test (two Python threads hammering tracks
  and tags, creating and dropping tracks, while stopped / raw / post-mix
  callbacks re-enter the mixer). `Destroy` / `MIX_Quit` / `MIX_Init` raise
  inside mixer callbacks; a track can't be changed from its own raw / cooked
  callback (undefined in SDL). A mixer wrapper that dies on its own mixing
  thread (the post-mix callback held the last reference), or a device mixer
  dropped off the main thread, is freed later on the main thread
  (`Py_AddPendingCall`; cancelled if `MIX_Quit` frees it first).
- **Callbacks** use the token pattern (`_callbacks`: token -> per-slot
  callables, dropped before the object goes); a pending exception is set
  aside around them. PCM callbacks get the float32 samples as `bytes` and may
  return same-sized bytes to replace them (anything else is reported and
  ignored); spec is always F32.
- **Lifetimes**: Track / Group wrappers hold their Mixer; `Mixer.Destroy`
  (and the final `MIX_Quit`) null them, waiting for in-flight calls, and keep
  their inputs alive until SDL has freed the mixer — `MIX_DestroyMixer` still
  touches each track's AudioStream while stopping it (valgrind caught the
  early release).
- **SDL quirks found**: `MIX_SetTrackOutputChannelMap(track, NULL, 0)` fails
  ("Wrong number of channels": SDL checks the count even for a NULL map), so
  `SetOutputChannelMap(None)` retries with the mixer's channels, then 2, then
  1 (stereo / 3D modes). Finite `MIX_CreateSineWaveAudio` tones end on one NaN
  sample frame (SDL_mixer 3.2.4 + SDL 3.4.18; valgrind flags an uninitialised
  read inside SDL called from SDL_mixer at that point), which poisons a mix;
  the example uses raw PCM notes. `halt_when_exhausted=False` keeps a track
  "playing" (silence) after its input and any fade-out end.
- **Not wrapped, deliberately**: `MIX_LoadAudioNoCopy` / `MIX_LoadRawAudioNoCopy`
  (SDL would keep reading Python's buffer), the `*_Properties` getters beyond
  audio metadata and the mixer's device id, `MIX_GetTrackAudio` /
  `MIX_GetTrackAudioStream` as SDL pointers (the wrapper returns the object it
  was given instead).

## Explicitly out of scope

Threads / mutexes / semaphores / condition vars / atomics (use Python's),
`SDL_Log*`, assertions, `SDL_main` / main callbacks, platform-specific APIs
(Android / iOS / WinRT), `GetWindowWMInfo`, stdinc shims. Further satellite
libraries (`SDL3_net`, ...) are a separate effort; if added, follow the
Phase 14 / 16 pattern (an `OPTIONAL` row in `setup.py`, one
`#ifdef PYSDL_HAVE_<LIB>` file). SDL3_ttf (phase 14) and SDL3_mixer
(phase 16) are built in when present.

Removed by SDL3 with no in-style replacement (dropped from the binding in the
SDL3 port): gesture recognition (`MULTIGESTURE` / `DOLLAR*`, `RecordGesture`,
dollar templates), window gamma ramp / brightness, `Texture.GL_Bind`,
`GetDisplayDPI`, `IMG_Init` / `IMG_Quit`, `GetRendererInfo` /
`GetRenderDriverInfo` (now name + properties), `RenderTargetSupported`,
`SDL_QueryTexture` (now `Texture.Query` over size + properties),
`RenderSetIntegerScale`, `AudioInit` / `AudioQuit`, `VideoInit` / `VideoQuit`,
the YUV conversion mode, `CreateShapedWindow` and the shape modes,
`CreateWindowFrom`. The new SDL3 subsystems
(GPU, camera, pen, async I/O, dialogs, tray) are future work.

## Per-phase checklist

Each phase ships: new constants, `.tp_doc` + terse method docstrings matching the
existing style, one `example/` script, one headless `tests/test_<area>.py`,
and a README coverage note.
