# SDL2 → SDL3 migration plan

This binding is a thin hand-written wrapper over SDL2 (+ SDL2_image). SDL3 renamed
or resignatured almost every entry point it calls, so this is a sweep across every
`src/*.c` file, the ~900-entry constant table, all 17 test modules, and all 14
example scripts — not an additive phase like those in `ROADMAP.md`.

The work is tracked on the `feature/sdl3` branch, one commit per phase.

## Why this is large

- **Error idiom flips everywhere.** SDL3 functions return `bool` (`true` = success)
  instead of `int` (`0` = success, `< 0` = error). Every
  `if (0 > SDL_X(...))` / `if (SDL_FALSE == SDL_X(...))` becomes `if (!SDL_X(...))`.
  Counter-returning calls (`SDL_GetAudioStreamData`, `SDL_PeepEvents`,
  `SDL_GetNumJoystickAxes`) still return `int` — this cannot be a blind sed.
- **`_constants.c`** needs regeneration: whole families renamed
  (`SDL_KEYDOWN` → `SDL_EVENT_KEY_DOWN`, `KMOD_*` → `SDL_KMOD_*`,
  `SDL_CONTROLLER_*` → `SDL_GAMEPAD_*`, `AUDIO_S16LSB` → `SDL_AUDIO_S16`,
  `SDL_SYSTEM_CURSOR_ARROW` → `SDL_SYSTEM_CURSOR_DEFAULT`,
  `SDL_PIXELFORMAT_RGB888` → `SDL_PIXELFORMAT_XRGB8888`).
- **`SDL_RWops` → `SDL_IOStream`** in the three loaders
  (`SDL_RWFromConstMem` → `SDL_IOFromConstMem`, `*_RW` → `*_IO`).
- **Audio is a ground-up rewrite.** The callback-fill model
  (`callback(stream, len)` → `memcpy`) is gone; SDL3 audio flows through
  `SDL_AudioStream`. `SDL_QueueAudio` / `SDL_DequeueAudio` /
  `SDL_LockAudioDevice` / `SDL_GetAudioDeviceStatus` / `SDL_AudioInit` all
  removed.
- **Device subsystems** (joystick, gamepad, haptic, sensor) move from a
  device-index model to an instance-ID model
  (`SDL_NumJoysticks()` / `SDL_JoystickOpen(index)` →
  `SDL_GetJoysticks(&count)` / `SDL_OpenJoystick(SDL_JoystickID)`).

## Decisions (locked)

| # | Decision | Choice |
|---|---|---|
| 1 | Module name `SDL2` → `SDL3` (forces `PyInit_SDL2` → `PyInit_SDL3`, all `import SDL2`, `setup.py` name) | **Rename.** Clean break, no alias. |
| 2 | Constant naming: SDL3 names verbatim-minus-`SDL_` (`EVENT_KEY_DOWN`) vs. legacy short (`KEYDOWN`) | **SDL3 names.** Optional thin alias block for the handful the examples use most. |
| 3 | `Audio` Python API redesign | **`Audio` wraps `SDL_OpenAudioDeviceStream` + push model.** `Audio.Open(callback=…)` stays recognizable; the callback signature changes and is documented. |
| 4 | `GameController` type → `Gamepad` | **Rename to `Gamepad`,** keep `GameController = Gamepad` alias for one release. |
| 5 | Accept feature drops with no clean SDL3 equivalent | **Yes** — see "Dropped" below. |
| 6 | `Init()` default (`SDL_INIT_EVERYTHING` gone) | Default to `VIDEO | AUDIO | EVENTS | JOYSTICK | GAMEPAD`; expose a synthesized `INIT_EVERYTHING`. |
| 7 | Debian packaging (`libsdl3-dev` is Trixie+) | Keep the deb config; document the distro requirement bump. |

## Dropped (removed by SDL3, no in-style replacement)

- Gesture recognition: `SDL_MULTIGESTURE` / `SDL_DOLLARGESTURE` / `SDL_DOLLARRECORD`
  events, `RecordGesture`, `LoadDollarTemplates` / `SaveDollarTemplate` /
  `SaveAllDollarTemplates`.
- Window gamma ramp: `Set/GetWindowGammaRamp`.
- `Texture.GL_Bind` / `GL_Unbind` (`SDL_GL_BindTexture` removed).
- `GetDisplayDPI` (`SDL_GetDisplayDPI` removed — `SDL_GetDisplayContentScale`
  is the nearest, different semantics).
- `IMG_Init` / `IMG_Quit` and `IMG_INIT_*` (SDL3_image auto-detects).
- `GetRendererInfo` / `GetRenderDriverInfo` and the `SDL_RendererInfo` struct
  (replaced by `SDL_GetRendererName` + renderer properties).
- `RenderTargetSupported` (`SDL_RenderTargetSupported` removed — always true).
- `SDL_QueryTexture` (size via `SDL_GetTextureSize`, format/access via
  properties).
- `RenderSetIntegerScale` (folded into `SDL_SetRenderLogicalPresentation`
  modes).
- CPU probes removed by SDL3: `Has3DNow`, `HasRDTSC`, and (verify against the
  installed headers) `HasMMX`, `HasARMSIMD`.
- `AudioInit` / `AudioQuit`, `SDL_MIX_MAXVOLUME` (volume is now a float 0–1),
  `SDL_AUDIO_ALLOW_*_CHANGE` flags.

## Behavior changes visible to Python callers

- Mouse / touch / wheel coordinates are `float`.
- Window events are individual event types (`EVENT_WINDOW_RESIZED`, …) — there is
  no `.window.event` sub-type field any more.
- `RenderReadPixels` returns a `Surface`.
- `CreateWindow` ignores the requested x/y (SDL3 positions via a follow-up call);
  the binding issues `SDL_SetWindowPosition` when a position is given.
- `RenderGeometry` vertex colors are 0–1 floats (`SDL_FColor`).
- `GetTicks()` returns a 64-bit value; `GetTicks64` is gone.
- `StartTextInput` / `StopTextInput` / `SetTextInputArea` and
  `SetRelativeMouseMode` take a `Window` argument (SDL3 made these per-window).
- `Init()` with no argument no longer starts every subsystem — see decision 6.

## Phases

Each phase ends with a clean compile and a green (or explicitly xfail-marked)
`python3 -m pytest`. Mirrors the per-phase checklist in `ROADMAP.md`:
constants, `.tp_doc` + docstrings, one example, one test module, a README note.

### Phase A — Toolchain & build

- `src/pysdl.h`: includes → `SDL3/SDL.h`, `SDL3_image/SDL_image.h`,
  `SDL3/SDL_vulkan.h`.
- `setup.py`: libraries `SDL3` / `SDL3_image`; probe `pkg-config sdl3 sdl3-image`
  (seed `PKG_CONFIG_PATH` with the Homebrew keg `pkgconfig` dirs); keep the
  `/usr/local` + `/opt/homebrew` fallback; rename the distribution to `SDL3` and
  bump the version.
- `src/pysdl.c`: `PyInit_SDL2` → `PyInit_SDL3`, module def name, `"SDL2.error"`
  → `"SDL3.error"`.
- `tools/gen_constants.py`: scan the `SDL3/` header dir, `pkg-config sdl3`,
  widen the name regex to `SDL_KMOD_` / `SDL_AUDIO_`, refresh the skip list and
  the `module_name()` rename rules.

Compilation does **not** succeed at the end of Phase A — every later phase still
uses SDL2 APIs. The deliverable is: the build system targets SDL3 and
`gen_constants.py` runs against the SDL3 headers so Phase B can proceed.

### Phase B — Constants

Regenerate `src/_constants.c` from the SDL3 headers. Drop removed families
(`INIT_NOPARACHUTE`, `WINDOW_SHOWN`, `WINDOW_FULLSCREEN_DESKTOP`, `IMG_INIT_*`,
`SDL_QUERY` / `IGNORE` / `ENABLE`, gesture events, `AUDIO_ALLOW_*`,
`MIX_MAXVOLUME`, most `SDL_RENDERER_*` flags). Re-guard enum members with
`SDL_VERSION_ATLEAST(3, x, 0)`. Rewrite `tests/test_constants.py`.

### Phase C — Cross-cutting sweep + `pysdl_util.c`

`SDL_bool` → `bool`, `SDL_TRUE` / `SDL_FALSE` → `true` / `false`,
`IMG_GetError` → `SDL_GetError`, the local `_raise()` helpers, the error-idiom
flip (reviewed per call). `PyToPixel` / `MapRGBA` gain the
`const SDL_PixelFormatDetails *` + `SDL_Palette *` signature. RWops → IOStream in
`LoadBMP` / `LoadImage` / `LoadWAV`. `tp_name` and error strings `"SDL2."` →
`"SDL3."`. `GetBasePath` stops freeing its result.

### Phase D — Graphics core

`Video`, `Window`, `Renderer`, `Texture`, `Surface`, `PixelFormat`, `Palette`,
`Cursor`.

- `SDL_CreateWindow(title, w, h, flags)` + optional `SDL_SetWindowPosition`;
  drop `SDL_WINDOW_SHOWN`; window flags widen to `Uint64`.
- `SDL_CreateRenderer(window, name)` (no index / flags) + `SDL_SetRenderVSync`.
- Draw primitives → `SDL_RenderPoint` / `Line` / `Rect` / `FillRect` (+ `s`
  plural), all `SDL_FRect` / `SDL_FPoint`. `RenderCopy*` →
  `SDL_RenderTexture` / `SDL_RenderTextureRotated` (float only) — collapse the
  four `Copy` variants to aliases.
- `RenderSet*` → `SDL_SetRender*`; `RenderSetLogicalSize` →
  `SDL_SetRenderLogicalPresentation` (needs a mode arg); drop
  `RenderSetIntegerScale`, `RenderTargetSupported`.
- `GetRendererInfo` / `GetRenderDriverInfo` → rewrite over
  `SDL_GetRendererName` + `SDL_GetRendererProperties` / `SDL_GetRenderDriver`.
- `RenderReadPixels` → returns `SDL_Surface *`; adapt.
- `Texture`: `SDL_QueryTexture` gone → `SDL_GetTextureSize` (floats) +
  properties; drop `GL_Bind`.
- `Surface`: `.format` is now the enum; bpp / masks via
  `SDL_GetPixelFormatDetails`; `SDL_FreeSurface` → `SDL_DestroySurface`;
  `SDL_MapRGB` → `SDL_MapSurfaceRGB`; `BlitScaled` / `SoftStretch` take an
  `SDL_ScaleMode`; `SDL_FillRect` → `SDL_FillSurfaceRect`; `SDL_ConvertSurface`
  new signature.
- `PixelFormat`: rewrite — hold `const SDL_PixelFormatDetails *` (from
  `SDL_GetPixelFormatDetails`, never freed) + an owned `SDL_Palette *`;
  `SDL_SetPixelFormatPalette` is gone.
- `Palette`: `SDL_AllocPalette` → `SDL_CreatePalette`, `SDL_FreePalette` →
  `SDL_DestroyPalette`.
- `Cursor`: `SDL_FreeCursor` → `SDL_DestroyCursor`; system-cursor enum renames.
- Displays: `SDL_GetDisplays(&count)` → `SDL_DisplayID`s; `SDL_Get*DisplayMode`
  return `const SDL_DisplayMode *` (`refresh_rate` is float); drop
  `GetDisplayDPI`.
- `SDL_GL_DeleteContext` → `SDL_GL_DestroyContext`;
  `SDL_Vulkan_GetInstanceExtensions(&count)` loses the window arg;
  `SDL_Vulkan_CreateSurface` gains an allocator arg.

### Phase E — Events & input

Rewrite `_event()`: type constants renamed; `keysym.sym` / `scancode` / `mod` →
`key` / `scancode` / `mod`, `.state` → `.down`; `SDL_WINDOWEVENT` → per-type
`SDL_EVENT_WINDOW_*` range handling; `SDL_DISPLAYEVENT` split; mouse fields → `f`;
controller union `caxis` / `cbutton` / `cdevice` → `gaxis` / `gbutton` /
`gdevice`; `adevice.iscapture` → `.recording`; **delete gesture cases**;
`drop.file` → `drop.data` (SDL owns it — remove the `consume` / `SDL_free`
logic). `SDL_EventState` / `SDL_QUERY` → `SDL_SetEventEnabled` /
`SDL_EventEnabled`. Event-filter callback returns `bool`. Poll / Wait / Push
return `bool`.

Input: `SDL_GetKeyboardState` returns `const bool *`; `SDL_GetMouseState` x / y
are `float *`; `SDL_StartTextInput` / `StopTextInput` / `SetTextInputArea` need a
window arg (module-function API change); `SDL_SetRelativeMouseMode` →
`SDL_SetWindowRelativeMouseMode(window, …)`; touch enumeration via
`SDL_GetTouchDevices` / `SDL_GetTouchFingers`; **delete `RecordGesture` + the
dollar-template functions** (removes the last RWops uses).

### Phase F — Audio

`pysdl_Audio.c` rewrite + `pysdl_AudioStream.c`.

New `SDL_AudioSpec` (`format` / `channels` / `freq` only). `Audio.Open` →
`SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK | id, &spec)` +
`SDL_OpenAudioDeviceStream` for the callback path (new `SDL_AudioStreamCallback`
shape). `Queue` / `Dequeue` / `ClearQueued` / `GetQueueSize` reimplemented over a
bound stream (`SDL_PutAudioStreamData` / `SDL_GetAudioStreamData` /
`SDL_GetAudioStreamQueued` / `SDL_ClearAudioStream`). `Pause(bool)` →
`SDL_PauseAudioDevice` / `SDL_ResumeAudioDevice`; `GetStatus` →
`SDL_AudioDevicePaused`. `Lock` / `Unlock` → `SDL_LockAudioStream`. `LoadWAV` →
`SDL_LoadWAV_IO` (bool, no `samples`), `SDL_FreeWAV` → `SDL_free`.
`MixAudioFormat` → `SDL_MixAudio` (float volume). Device queries →
`SDL_GetAudioPlaybackDevices` / `SDL_GetAudioDeviceFormat`. Drop `AudioInit` /
`AudioQuit`. `AudioStream`: `SDL_CreateAudioStream(&src, &dst)` + the
`SDL_*AudioStream*` renames; keep the six-int constructor.

### Phase G — Input devices

`Joystick`, `Gamepad` (renamed from `GameController`), `Haptic`, `Sensor`.

All switch from device-index to instance-ID enumeration (`SDL_GetJoysticks` /
`SDL_GetHaptics` / `SDL_GetSensors` returning ID arrays). Mass method renames
(`SDL_JoystickGetAxis` → `SDL_GetJoystickAxis`, `SDL_GameController*` →
`SDL_*Gamepad*`, `SDL_HapticNewEffect` → `SDL_CreateHapticEffect`,
`SDL_SensorGetData` → `SDL_GetSensorData`, …). `*EventState` →
`SDL_Set*EventsEnabled`. Power level → `SDL_GetJoystickPowerInfo`. Virtual
joystick now takes a descriptor struct. `git mv` the `.c` file, its test, and its
example so history follows.

### Phase H — Timers / system misc

`GetTicks` → 64-bit, drop `GetTicks64`. `SDL_AddTimer` callback signature
(`(userdata, timerID, interval)`). Screensaver / clipboard bool returns;
`SDL_IsScreenSaverEnabled` → `SDL_ScreenSaverEnabled`. `SDL_GetCPUCount` →
`SDL_GetNumLogicalCPUCores`; drop the removed `Has*`; `SDL_SIMDGetAlignment` →
`SDL_GetSIMDAlignment`. `Version` / `GetVersion` over the packed-int
`SDL_GetVersion()` + `SDL_VERSIONNUM_*` macros.

### Phase I — Examples

Port all 14: `draw`, `keyboard`, `gamepad`, `audio`, `wav`, `events`, `window`,
`timer`, `info`, `surface`, `rects`, `simple`, `adjust`.

### Phase J — Tests

`conftest.py` (`import SDL3`, module glob, drop `INIT_TIMER`) + all 17
`test_*.py`. Run the headless suite; triage.

### Phase K — Docs & packaging

`README.rst` (`brew install sdl3 sdl3_image`, distro notes), rewrite the
`ROADMAP.md` intro + out-of-scope, `setup.py` version, `CLAUDE.md`.

## Sequencing notes

- Phases A–C unblock everything; do them first.
- D, E, G are large but mechanical once the pattern is set.
- F (audio) is the highest risk — genuine redesign; decision 3 is the contract.
- One commit per phase on `feature/sdl3`.
