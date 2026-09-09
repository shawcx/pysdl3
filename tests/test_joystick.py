'''Joystick and Gamepad, driven by a virtual joystick (SDL3).

SDL3 identifies joysticks by SDL_JoystickID; the virtual-joystick attach returns
that id directly.'''

import pytest


@pytest.fixture
def joystick_id(sdl):
    sdl.InitSubSystem(sdl.INIT_JOYSTICK | sdl.INIT_GAMEPAD)
    jid = sdl.JoystickAttachVirtual(sdl.JOYSTICK_TYPE_GAMEPAD, 4, 6, 1)
    yield jid
    try:
        sdl.JoystickDetachVirtual(jid)
    except sdl.error:
        pass


@pytest.fixture
def joystick(sdl, joystick_id):
    js = sdl.Joystick(joystick_id)
    yield js
    js.Close()


def test_module_functions(sdl, joystick_id):
    assert sdl.NumJoysticks() >= 1
    assert joystick_id in sdl.GetJoysticks()
    assert sdl.JoystickIsVirtual(joystick_id) is True
    assert sdl.JoystickNameForIndex(joystick_id)
    assert isinstance(sdl.JoystickEventState(), bool)
    sdl.JoystickUpdate()


def test_joystick_metadata(joystick):
    assert joystick.Name()
    assert len(joystick.GetGUID()) == 32
    assert isinstance(joystick.InstanceID(), int)
    assert joystick.Attached() is True
    assert (joystick.NumAxes(), joystick.NumButtons(), joystick.NumHats()) == (4, 6, 1)


def test_virtual_input_reflects_in_getters(sdl, joystick):
    joystick.SetVirtualAxis(1, -20000)
    joystick.SetVirtualButton(3, 1)
    joystick.SetVirtualHat(0, sdl.HAT_LEFT)
    sdl.JoystickUpdate()

    assert joystick.GetAxis(1) == -20000
    assert joystick.GetButton(3) is True
    assert joystick.GetButton(0) is False
    assert joystick.GetHat(0) == sdl.HAT_LEFT


def test_rumble_and_led_do_not_raise(joystick):
    assert isinstance(joystick.Rumble(0xFFFF, 0x8000, 50), bool)
    assert isinstance(joystick.SetLED(10, 20, 30), bool)
    assert isinstance(joystick.HasLED(), bool)


def test_uninitialised_joystick_is_guarded(sdl):
    with pytest.raises(sdl.error):
        sdl.Joystick().Name()


def test_close_is_idempotent(sdl, joystick_id):
    js = sdl.Joystick(joystick_id)
    js.Close()
    js.Close()
    with pytest.raises(sdl.error):
        js.NumAxes()


# --- gamepad --------------------------------------------------------------------

@pytest.fixture
def controller(sdl, joystick_id):
    if not sdl.IsGameController(joystick_id):
        pytest.skip('virtual joystick has no gamepad mapping here')
    gc = sdl.Gamepad(joystick_id)
    yield gc
    gc.Close()


def test_controller_basics(sdl, controller):
    assert controller.Name()
    assert controller.Attached() is True
    assert isinstance(controller.GetAxis(sdl.GAMEPAD_AXIS_LEFTX), int)
    assert isinstance(controller.GetButton(sdl.GAMEPAD_BUTTON_SOUTH), bool)
    assert isinstance(controller.Mapping(), str)


def test_controller_get_joystick_is_borrowed(sdl, controller, joystick_id):
    js = controller.GetJoystick()
    assert type(js).__name__ == 'Joystick'
    del js  # borrowed: must not close the gamepad's joystick
    assert controller.GetButton(sdl.GAMEPAD_BUTTON_EAST) is False


def test_controller_add_mapping(sdl):
    guid = '03000000f.0000000000000000000000'.replace('.', '0')
    rc = sdl.GameControllerAddMapping(f'{guid},pysdl3 test pad,a:b0,b:b1,')
    assert rc in (0, 1)
