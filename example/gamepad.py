#!/usr/bin/env python3

# Gamepad / joystick monitor. Plug in a pad and it opens automatically: it
# prints the device info and button bindings, then live input - sticks,
# buttons, touchpad, gyro, battery. Pressing a gamepad button rumbles it.
# Ctrl-C or close the window to quit.
#
# With no real device, pass --virtual to attach a virtual gamepad (with a
# touchpad and a gyro) that reports the rumble / LED / player requests it
# receives. Drive it with the number keys 0-5 (axes), b (press / release a
# button), t (touch / lift the touchpad) and g (gyro sample). --demo does the same unattended and then quits, so it
# runs headless (SDL_VIDEO_DRIVER=dummy).

import sys
import SDL3

DEMO = '--demo' in sys.argv
VIRTUAL = DEMO or '--virtual' in sys.argv

# Keep joystick input flowing while another window has focus.
SDL3.SetHint('SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS', '1')
SDL3.Init(SDL3.INIT_VIDEO | SDL3.INIT_JOYSTICK | SDL3.INIT_GAMEPAD)

AXIS_NAMES = {
    SDL3.GAMEPAD_AXIS_LEFTX: 'LeftX', SDL3.GAMEPAD_AXIS_LEFTY: 'LeftY',
    SDL3.GAMEPAD_AXIS_RIGHTX: 'RightX', SDL3.GAMEPAD_AXIS_RIGHTY: 'RightY',
    SDL3.GAMEPAD_AXIS_LEFT_TRIGGER: 'TrigL', SDL3.GAMEPAD_AXIS_RIGHT_TRIGGER: 'TrigR',
}
POWER = {SDL3.POWERSTATE_ERROR: 'error', SDL3.POWERSTATE_UNKNOWN: '?',
         SDL3.POWERSTATE_ON_BATTERY: 'battery', SDL3.POWERSTATE_NO_BATTERY: 'wired',
         SDL3.POWERSTATE_CHARGING: 'charging', SDL3.POWERSTATE_CHARGED: 'charged'}

window = SDL3.Window('gamepad', (360, 120))

controllers = {}   # instance id -> Gamepad
joysticks = {}     # instance id -> Joystick


def side(bind):
    '''One side of a GetBindings() entry -> 'b0' / 'a1' / 'h0.4'.'''
    if bind is None:
        return '-'
    kind, index = bind[0], bind[1]
    if kind == SDL3.GAMEPAD_BINDTYPE_BUTTON:
        return f'b{index}'
    if kind == SDL3.GAMEPAD_BINDTYPE_AXIS:
        return f'a{index}'
    return f'h{index}.{bind[2]}'


def add_device(iid):
    # A gamepad sends both JOYSTICK_ADDED and GAMEPAD_ADDED; open it once.
    if iid in controllers or iid in joysticks:
        return
    if SDL3.IsGamepad(iid):
        gc = SDL3.Gamepad(iid)
        controllers[iid] = gc
        gc.SetPlayerIndex(len(controllers) - 1)
        state, percent = gc.GetPowerInfo()
        print(f'+ gamepad {iid}: {gc.Name()!r}  {gc.GetVendor():04x}:{gc.GetProduct():04x}  '
              f'{SDL3.GetGamepadStringForType(gc.GetType())}  player {gc.GetPlayerIndex()}  '
              f'touchpads {gc.GetNumTouchpads()}  power {POWER.get(state, state)} {percent}%')
        for inp, out in gc.GetBindings():
            if out is None:
                continue
            if out[0] == SDL3.GAMEPAD_BINDTYPE_BUTTON:
                name = SDL3.GetGamepadStringForButton(out[1])
            else:
                name = SDL3.GetGamepadStringForAxis(out[1])
            print(f'    {name:>13} <- {side(inp)}')
        if gc.HasSensor(SDL3.SENSOR_GYRO):
            gc.SetSensorEnabled(SDL3.SENSOR_GYRO, True)
        if gc.HasLED():
            gc.SetLED(0, 64, 255)
    else:
        js = SDL3.Joystick(iid)
        joysticks[iid] = js
        print(f'+ joystick {iid}: {js.Name()!r}  '
              f'{js.NumAxes()} axes, {js.NumButtons()} buttons, {js.NumHats()} hats')


virtual_id = vjs = None
if VIRTUAL:
    virtual_id = SDL3.JoystickAttachVirtualEx(
        SDL3.JOYSTICK_TYPE_GAMEPAD, 6, 8, 1, name='pysdl3 virtual pad',
        touchpads=[1], sensors=[(SDL3.SENSOR_GYRO, 100.0)],
        rumble=lambda low, high: print(f'    (virtual pad rumble {low}/{high})'),
        set_led=lambda r, g, b: print(f'    (virtual pad LED {r},{g},{b})'),
        set_player_index=lambda player: print(f'    (virtual pad is player {player})'))
    vjs = SDL3.Joystick(virtual_id)
    print('attached a virtual gamepad; keys 0-5 nudge its axes, b toggles a button, '
          't toggles a touchpad finger, g sends a gyro sample')

pressed = touching = False


def virtual_key(sym):
    '''Drive the virtual pad from a key press.'''
    global pressed, touching
    if SDL3.K_0 <= sym <= SDL3.K_5:
        vjs.SetVirtualAxis(sym - SDL3.K_0, 20000)
    elif sym == SDL3.K_B:
        pressed = not pressed
        vjs.SetVirtualButton(0, pressed)
    elif sym == SDL3.K_T:
        touching = not touching
        vjs.SetVirtualTouchpad(0, 0, touching, 0.3, 0.6, 1.0 if touching else 0.0)
    elif sym == SDL3.K_G:
        try:
            vjs.SendVirtualSensorData(SDL3.SENSOR_GYRO, (2.0, -1.5, 0.5))
        except SDL3.error as err:  # needs SDL >= 3.2.22
            print(f'    (gyro: {err})')


# --demo: one scripted key per loop pass, then quit.
script = [SDL3.K_0, SDL3.K_B, SDL3.K_B, SDL3.K_T, SDL3.K_T, SDL3.K_G] if DEMO else []

for iid in SDL3.GetJoysticks():
    add_device(iid)

running = True
while running:
    event = SDL3.WaitEventTimeout(50) if DEMO else SDL3.WaitEvent()
    if event is None:
        if DEMO:
            if not script:
                break
            virtual_key(script.pop(0))
        continue
    kind, data = event

    if kind == SDL3.EVENT_QUIT:
        running = False

    elif kind in (SDL3.EVENT_GAMEPAD_ADDED, SDL3.EVENT_JOYSTICK_ADDED):
        add_device(data[0])
    elif kind in (SDL3.EVENT_GAMEPAD_REMOVED, SDL3.EVENT_JOYSTICK_REMOVED):
        controllers.pop(data[0], None)
        joysticks.pop(data[0], None)
        print(f'- device {data[0]} removed')

    elif kind == SDL3.EVENT_GAMEPAD_AXIS_MOTION:
        which, axis, value = data
        if abs(value) > 8000:
            print(f'  [{which}] {AXIS_NAMES.get(axis, axis):>6} = {value:+6d}')
    elif kind in (SDL3.EVENT_GAMEPAD_BUTTON_DOWN, SDL3.EVENT_GAMEPAD_BUTTON_UP):
        which, button, state = data
        gc = controllers.get(which)
        if gc and state:
            gc.Rumble(0x8000, 0x8000, 120)
        print(f'  [{which}] button {SDL3.GetGamepadStringForButton(button)} '
              f'{"down" if state else "up"}')

    elif kind in (SDL3.EVENT_GAMEPAD_TOUCHPAD_DOWN, SDL3.EVENT_GAMEPAD_TOUCHPAD_UP):
        which, pad, finger, x, y, pressure = data
        verb = 'down' if kind == SDL3.EVENT_GAMEPAD_TOUCHPAD_DOWN else 'up'
        print(f'  [{which}] touchpad {pad} finger {finger} {verb} at ({x:.2f}, {y:.2f})')
    elif kind == SDL3.EVENT_GAMEPAD_SENSOR_UPDATE:
        which, sensor, (x, y, z), timestamp = data
        if max(abs(x), abs(y), abs(z)) > 1.0:
            print(f'  [{which}] gyro {x:+.1f} {y:+.1f} {z:+.1f}')
    elif kind == SDL3.EVENT_JOYSTICK_BATTERY_UPDATED:
        which, state, percent = data
        print(f'  [{which}] battery {POWER.get(state, state)} {percent}%')

    elif kind == SDL3.EVENT_JOYSTICK_HAT_MOTION:
        print(f'  [{data[0]}] hat {data[1]} -> {data[2]}')
    elif kind in (SDL3.EVENT_JOYSTICK_BUTTON_DOWN, SDL3.EVENT_JOYSTICK_BUTTON_UP):
        if data[0] not in controllers:
            print(f'  [{data[0]}] joy button {data[1]} {"down" if data[2] else "up"}')

    elif kind == SDL3.EVENT_KEY_DOWN:
        if data[2] == SDL3.K_ESCAPE:
            running = False
        elif vjs is not None:
            virtual_key(data[2])

controllers.clear()
joysticks.clear()
if virtual_id is not None:
    vjs.Close()
    SDL3.JoystickDetachVirtual(virtual_id)
SDL3.Quit()
