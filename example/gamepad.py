#!/usr/bin/env python3

# Game-controller / joystick monitor. Plug in a pad and it opens automatically;
# move sticks and press buttons to see live events. Ctrl-C to quit.
#
# With no real device, pass --virtual to attach a virtual joystick and drive it
# with the number keys 0-5 (needs SDL >= 2.0.14).

import sys
import SDL3

SDL3.Init(SDL3.INIT_VIDEO | SDL3.INIT_JOYSTICK | SDL3.INIT_GAMECONTROLLER)

AXIS_NAMES = {
    SDL3.GAMEPAD_AXIS_LEFTX: 'LeftX', SDL3.GAMEPAD_AXIS_LEFTY: 'LeftY',
    SDL3.GAMEPAD_AXIS_RIGHTX: 'RightX', SDL3.GAMEPAD_AXIS_RIGHTY: 'RightY',
    SDL3.GAMEPAD_AXIS_TRIGGERLEFT: 'TrigL', SDL3.GAMEPAD_AXIS_TRIGGERRIGHT: 'TrigR',
}

window = SDL3.Window('gamepad', (360, 120))

controllers = {}   # instance id -> GameController
joysticks = {}     # instance id -> Joystick


def add_device(device_index):
    if SDL3.IsGameController(device_index):
        gc = SDL3.Gamepad(device_index)
        iid = gc.GetJoystick().InstanceID()
        if iid in controllers:
            return
        controllers[iid] = gc
        print(f'+ controller {iid}: {gc.Name()!r}')
    else:
        js = SDL3.Joystick(device_index)
        if js.InstanceID() in joysticks:
            return
        joysticks[js.InstanceID()] = js
        print(f'+ joystick {js.InstanceID()}: {js.Name()!r}  '
              f'{js.NumAxes()} axes, {js.NumButtons()} buttons, {js.NumHats()} hats')


virtual_index = None
if '--virtual' in sys.argv and hasattr(SDL3, 'JoystickAttachVirtual'):
    virtual_index = SDL3.JoystickAttachVirtual(SDL3.JOYSTICK_TYPE_GAMEPAD, 6, 8, 1)
    vjs = SDL3.Joystick(virtual_index)
    print('attached virtual joystick; press number keys 0-5 to nudge its axes')

for device_index in SDL3.GetJoysticks():
    add_device(i)

running = True
while running:
    event = SDL3.WaitEvent()
    if event is None:
        continue
    kind, data = event

    if kind == SDL3.EVENT_QUIT:
        running = False

    elif kind in (SDL3.CONTROLLERDEVICEADDED, SDL3.JOYDEVICEADDED):
        add_device(data[0])
    elif kind in (SDL3.CONTROLLERDEVICEREMOVED, SDL3.JOYDEVICEREMOVED):
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
        if gc:
            gc.Rumble(0x8000, 0x8000, 120) if state else None
        print(f'  [{which}] button {button} {"down" if state else "up"}')

    elif kind == SDL3.EVENT_JOYSTICK_HAT_MOTION:
        print(f'  [{data[0]}] hat {data[1]} -> {data[2]}')
    elif kind in (SDL3.EVENT_JOYSTICK_BUTTON_DOWN, SDL3.EVENT_JOYSTICK_BUTTON_UP):
        print(f'  [{data[0]}] joy button {data[1]} {"down" if data[2] else "up"}')

    elif kind == SDL3.EVENT_KEY_DOWN and virtual_index is not None:
        sym = data[2]
        if SDL3.K_0 <= sym <= SDL3.K_5:
            axis = sym - SDL3.K_0
            vjs.SetVirtualAxis(axis, 20000)
        elif sym == SDL3.K_ESCAPE:
            running = False

if virtual_index is not None:
    SDL3.JoystickDetachVirtual(virtual_index)
SDL3.Quit()
