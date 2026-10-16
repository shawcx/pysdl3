'''Joystick and Gamepad, driven by a virtual joystick (SDL3).

SDL3 identifies joysticks by SDL_JoystickID; the virtual-joystick attach returns
that id directly.'''

import gc
import weakref

import pytest


@pytest.fixture(autouse=True)
def background_events(sdl):
    '''SDL ignores joystick input while windows exist but none has keyboard
    focus, which a window another test left alive (under a dummy driver that
    never gives focus) would cause: every virtual axis would read 0 here.'''
    name = 'SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS'
    previous = sdl.GetHint(name)
    sdl.SetHint(name, '1')
    yield
    sdl.SetHint(name, previous if previous is not None else '0')


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


def _drain(sdl):
    events = []
    while (event := sdl.PollEvent()) is not None:
        events.append(event)
    return events


def test_module_functions(sdl, joystick_id):
    assert sdl.NumJoysticks() >= 1
    assert joystick_id in sdl.GetJoysticks()
    assert sdl.JoystickIsVirtual(joystick_id) is True
    assert sdl.IsJoystickVirtual(joystick_id) is True
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
    with pytest.raises(sdl.error):
        sdl.Joystick().GetAxis(0)


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


def test_controller_get_joystick_is_owned(sdl, controller, joystick_id):
    js = controller.GetJoystick()
    assert type(js).__name__ == 'Joystick'
    assert js.InstanceID() == joystick_id
    js.Close()  # an owned reference: must not close the gamepad's joystick
    assert controller.GetButton(sdl.GAMEPAD_BUTTON_EAST) is False
    assert controller.Attached() is True


def test_controller_joystick_outlives_controller(sdl, joystick_id):
    if not sdl.IsGameController(joystick_id):
        pytest.skip('virtual joystick has no gamepad mapping here')
    gc = sdl.Gamepad(joystick_id)
    js = gc.GetJoystick()
    gc.Close()  # used to free the joystick behind the borrowed wrapper
    assert js.Attached() is True
    assert js.NumButtons() == 6
    js.Close()


def test_controller_add_mapping(sdl):
    guid = '03000000f.0000000000000000000000'.replace('.', '0')
    rc = sdl.GameControllerAddMapping(f'{guid},pysdl3 test pad,a:b0,b:b1,')
    assert rc in (0, 1)


# --- phase 10: joystick / gamepad completeness -----------------------------

BUTTONS = ('GAMEPAD_BUTTON_SOUTH', 'GAMEPAD_BUTTON_EAST')
AXES = ('GAMEPAD_AXIS_LEFTX', 'GAMEPAD_AXIS_LEFTY')


def _sensor_data_supported(sdl):
    '''SendVirtualSensorData needs SDL >= 3.2.22 (older releases overflow
    their sensor queue on a second sample).'''
    return tuple(sdl.GetVersion()) >= (3, 2, 22)


@pytest.fixture
def need_ex(sdl):
    sdl.InitSubSystem(sdl.INIT_JOYSTICK | sdl.INIT_GAMEPAD)


@pytest.fixture
def calls():
    return []


@pytest.fixture
def vpad(sdl, need_ex, calls):
    '''A virtual gamepad with South/East, LeftX/LeftY and recording callbacks.'''
    jid = sdl.JoystickAttachVirtualEx(
        sdl.JOYSTICK_TYPE_GAMEPAD, 2, 2, 0,
        vendor_id=0x1234, product_id=0x5678, name='pysdl3 vpad',
        button_mask=sum(1 << getattr(sdl, b) for b in BUTTONS),
        axis_mask=sum(1 << getattr(sdl, a) for a in AXES),
        rumble=lambda low, high: calls.append(('rumble', low, high)),
        set_led=lambda r, g, b: calls.append(('led', r, g, b)),
        send_effect=lambda data: calls.append(('effect', data)),
        set_player_index=lambda player: calls.append(('player', player)),
    )
    yield jid
    try:
        sdl.JoystickDetachVirtual(jid)
    except sdl.error:
        pass


def test_device_id_queries(sdl, vpad):
    js = sdl.Joystick(vpad)
    try:
        assert sdl.JoystickGetDeviceGUID(vpad) == js.GetGUID()
        assert sdl.GetJoystickGUIDForID(vpad) == js.GetGUID()
        assert sdl.JoystickGetDeviceVendor(vpad) == 0x1234
        assert sdl.GetJoystickProductForID(vpad) == 0x5678
        assert isinstance(sdl.JoystickGetDeviceProductVersion(vpad), int)
        assert sdl.JoystickGetDeviceType(vpad) == sdl.JOYSTICK_TYPE_GAMEPAD
        assert isinstance(sdl.JoystickGetDevicePlayerIndex(vpad), int)
        assert sdl.JoystickPathForIndex(vpad) is None  # virtual devices have no path
        assert sdl.GetJoystickNameForID(vpad) == 'pysdl3 vpad'
    finally:
        js.Close()


def test_joystick_info_methods(sdl, vpad):
    js = sdl.Joystick(vpad)
    try:
        assert (js.GetVendor(), js.GetProduct()) == (0x1234, 0x5678)
        assert isinstance(js.GetProductVersion(), int)
        assert js.GetType() == sdl.JOYSTICK_TYPE_GAMEPAD
        assert js.GetSerial() is None
        assert js.Path() is None
        assert isinstance(js.GetFirmwareVersion(), int)
        assert js.GetAxisInitialState(0) in (0, None)
        state, percent = js.GetPowerInfo()
        assert state == js.CurrentPowerLevel() and isinstance(percent, int)
        assert isinstance(js.GetConnectionState(), int)
    finally:
        js.Close()


def test_player_index_and_lookup(sdl, vpad, calls):
    js = sdl.Joystick(vpad)
    try:
        js.SetPlayerIndex(3)
        assert js.GetPlayerIndex() == 3
        assert ('player', 3) in calls

        found = sdl.JoystickFromPlayerIndex(3)
        assert found.InstanceID() == js.InstanceID()
        found.Close()  # owned reference: must not close `js`
        assert js.Attached() is True

        again = sdl.GetJoystickFromID(js.InstanceID())
        assert again.Name() == 'pysdl3 vpad'
        again.Close()
        assert js.Attached() is True
        assert sdl.JoystickFromInstanceID(0x7FFFFFF0) is None
    finally:
        js.Close()


def test_virtual_callbacks(sdl, vpad, calls):
    js = sdl.Joystick(vpad)
    try:
        assert js.HasRumble() is True
        assert js.HasRumbleTriggers() is False  # no rumble_triggers callback
        assert js.HasLED() is True
        assert js.Rumble(100, 200, 10) is True
        assert js.SetLED(1, 2, 3) is True
        assert js.SendEffect(b'\x01\x02') is True
        assert js.RumbleTriggers(1, 1, 10) is False  # no callback: unsupported
    finally:
        js.Close()
    # SDL may stop a still-rumbling joystick on close with rumble(0, 0).
    if calls[-1] == ('rumble', 0, 0):
        calls.pop()
    assert calls[-3:] == [('rumble', 100, 200), ('led', 1, 2, 3), ('effect', b'\x01\x02')]


def test_virtual_callback_failure_and_exception(sdl, need_ex, capfd):
    def boom(low, high):
        raise RuntimeError('rumble exploded')

    jid = sdl.JoystickAttachVirtualEx(rumble=boom, set_led=lambda r, g, b: False)
    js = sdl.Joystick(jid)
    try:
        assert js.Rumble(1, 1, 10) is False
        assert 'rumble exploded' in capfd.readouterr().err
        assert 'rumble callback failed' in sdl.GetError()
        assert js.SetLED(0, 0, 0) is False
        assert 'set_led callback failed' in sdl.GetError()
    finally:
        js.Close()
        sdl.JoystickDetachVirtual(jid)


def test_virtual_update_callback(sdl, need_ex):
    ticks = []
    jid = sdl.JoystickAttachVirtualEx(naxes=1, update=lambda: ticks.append(1))
    js = sdl.Joystick(jid)
    try:
        sdl.JoystickUpdate()
        assert ticks
    finally:
        js.Close()
        sdl.JoystickDetachVirtual(jid)


def test_detach_releases_callbacks(sdl, need_ex):
    class Callback:
        def __call__(self, low, high):
            pass

    cb = Callback()
    ref = weakref.ref(cb)
    jid = sdl.JoystickAttachVirtualEx(rumble=cb)
    del cb
    gc.collect()
    assert ref() is not None  # SDL may still call it
    sdl.DetachVirtualJoystick(jid)  # SDL's Cleanup callback drops it
    gc.collect()
    assert ref() is None


def test_callback_while_an_exception_unwinds(sdl, need_ex):
    '''Regression: closing a rumbling joystick from a dealloc during exception
    unwinding runs the rumble callback with the exception still pending; the
    callback must neither clobber nor swallow it.'''
    seen = []
    jid = sdl.JoystickAttachVirtualEx(rumble=lambda low, high: seen.append((low, high)))

    def rumbling():
        js = sdl.Joystick(jid)
        assert js.Rumble(1, 2, 60_000) is True
        return js

    try:
        with pytest.raises(ZeroDivisionError):
            (rumbling(), 1 / 0)  # the Joystick is freed while the error unwinds
        assert seen[-1] == (0, 0)  # SDL stopped the rumble on close
    finally:
        sdl.JoystickDetachVirtual(jid)


def test_attach_virtual_ex_validates(sdl, need_ex):
    with pytest.raises(TypeError):
        sdl.JoystickAttachVirtualEx(rumble=42)
    with pytest.raises(ValueError):
        sdl.JoystickAttachVirtualEx(naxes=256)  # SDL walks axes with a Uint8
    with pytest.raises(ValueError):
        sdl.JoystickAttachVirtualEx(touchpads=[0])
    with pytest.raises(ValueError):
        sdl.JoystickAttachVirtualEx(sensors=[(sdl.SENSOR_GYRO, 1.0, 2.0)])
    with pytest.raises(TypeError):
        sdl.JoystickAttachVirtualEx(sensors=[None])


def test_virtual_ball(sdl, need_ex):
    jid = sdl.AttachVirtualJoystick(sdl.JOYSTICK_TYPE_UNKNOWN, nballs=1)
    js = sdl.Joystick(jid)
    try:
        # SDL (3.2 - 3.4) never reports a virtual joystick's balls: NumBalls()
        # is 0 and no BALL_MOTION is sent, but the ball index is still checked.
        js.SetVirtualBall(0, 5, -3)
        with pytest.raises(sdl.error):
            js.SetVirtualBall(1, 0, 0)
        with pytest.raises(OverflowError):
            js.SetVirtualBall(0, 40000, 0)
    finally:
        js.Close()
        sdl.JoystickDetachVirtual(jid)


def test_guid_conversions(sdl, vpad):
    guid = sdl.JoystickGetDeviceGUID(vpad)
    raw = sdl.JoystickGetGUIDFromString(guid)
    assert isinstance(raw, bytes) and len(raw) == 16
    assert sdl.JoystickGetGUIDString(raw) == guid
    assert sdl.GUIDFromString(guid) == sdl.StringToGUID(guid) == raw
    assert sdl.GUIDToString(raw) == sdl.GUIDToString(guid) == guid
    with pytest.raises(ValueError):
        sdl.JoystickGetGUIDString(b'short')
    with pytest.raises(ValueError):
        sdl.JoystickGetGUIDFromString('abc')
    with pytest.raises(ValueError):
        sdl.StringToGUID('zz' * 16)
    with pytest.raises(TypeError):
        sdl.GUIDToString(42)
    vendor, product, version, crc16 = sdl.GetJoystickGUIDInfo(guid)
    assert (vendor, product) == (0x1234, 0x5678)


def test_lock_joysticks(sdl, need_ex):
    sdl.LockJoysticks()
    sdl.UnlockJoysticks()


@pytest.fixture
def vcontroller(sdl, vpad):
    gc = sdl.Gamepad(vpad)
    yield gc
    gc.Close()


def test_controller_capabilities(sdl, vcontroller):
    for name in BUTTONS:
        assert vcontroller.HasButton(getattr(sdl, name)) is True
    for name in AXES:
        assert vcontroller.HasAxis(getattr(sdl, name)) is True
    assert vcontroller.HasButton(sdl.GAMEPAD_BUTTON_WEST) is False
    assert vcontroller.HasAxis(sdl.GAMEPAD_AXIS_RIGHTX) is False
    assert vcontroller.HasLED() is True
    assert vcontroller.HasRumble() is True
    assert vcontroller.HasRumbleTriggers() is False


def test_controller_binds(sdl, vcontroller):
    kind, button = vcontroller.GetBindForButton(sdl.GAMEPAD_BUTTON_SOUTH)
    assert kind == sdl.GAMEPAD_BINDTYPE_BUTTON and button == 0
    kind, axis = vcontroller.GetBindForAxis(sdl.GAMEPAD_AXIS_LEFTY)
    assert kind == sdl.GAMEPAD_BINDTYPE_AXIS and axis == 1
    assert vcontroller.GetBindForButton(sdl.GAMEPAD_BUTTON_WEST) is None

    bindings = vcontroller.GetBindings()
    button, axis = sdl.GAMEPAD_BINDTYPE_BUTTON, sdl.GAMEPAD_BINDTYPE_AXIS
    assert ((button, 1), (button, sdl.GAMEPAD_BUTTON_EAST)) in bindings
    assert ((axis, 0, -32768, 32767), (axis, sdl.GAMEPAD_AXIS_LEFTX, -32768, 32767)) in bindings
    assert len(bindings) == 4


def test_controller_info(sdl, vcontroller, vpad, calls):
    assert vcontroller.InstanceID() == vpad
    assert (vcontroller.GetVendor(), vcontroller.GetProduct()) == (0x1234, 0x5678)
    assert isinstance(vcontroller.GetProductVersion(), int)
    assert isinstance(vcontroller.GetType(), int)
    assert isinstance(vcontroller.GetRealType(), int)
    assert vcontroller.GetSerial() is None
    assert vcontroller.Path() is None
    assert isinstance(vcontroller.GetFirmwareVersion(), int)
    assert vcontroller.GetSteamHandle() == 0
    symbol = vcontroller.GetAppleSFSymbolsNameForButton(sdl.GAMEPAD_BUTTON_SOUTH)
    assert symbol is None or isinstance(symbol, str)  # str only on Apple platforms
    symbol = vcontroller.GetAppleSFSymbolsNameForAxis(sdl.GAMEPAD_AXIS_LEFTX)
    assert symbol is None or isinstance(symbol, str)
    assert isinstance(vcontroller.GetButtonLabel(sdl.GAMEPAD_BUTTON_SOUTH), int)
    state, percent = vcontroller.GetPowerInfo()
    assert isinstance(state, int) and isinstance(percent, int)
    assert isinstance(vcontroller.GetConnectionState(), int)

    vcontroller.SetPlayerIndex(2)
    assert vcontroller.GetPlayerIndex() == 2
    assert vcontroller.Rumble(7, 8, 10) is True
    assert vcontroller.SendEffect(b'z') is True
    assert ('rumble', 7, 8) in calls and ('effect', b'z') in calls


def test_controller_sensors_and_touchpads_absent(sdl, vcontroller):
    assert vcontroller.HasSensor(sdl.SENSOR_ACCEL) is False
    assert vcontroller.IsSensorEnabled(sdl.SENSOR_ACCEL) is False
    assert vcontroller.GetSensorDataRate(sdl.SENSOR_ACCEL) == 0.0
    with pytest.raises(sdl.error):
        vcontroller.SetSensorEnabled(sdl.SENSOR_ACCEL, True)
    with pytest.raises(sdl.error):
        vcontroller.GetSensorData(sdl.SENSOR_ACCEL)
    with pytest.raises(ValueError):
        vcontroller.GetSensorData(sdl.SENSOR_ACCEL, count=0)

    assert vcontroller.GetNumTouchpads() == 0
    assert vcontroller.GetNumTouchpadFingers(0) == 0
    with pytest.raises(sdl.error):
        vcontroller.GetTouchpadFinger(0, 0)


@pytest.fixture
def sensor_pad(sdl, need_ex, calls):
    '''A virtual gamepad with one two-finger touchpad, a gyro and an accelerometer.'''
    jid = sdl.JoystickAttachVirtualEx(
        sdl.JOYSTICK_TYPE_GAMEPAD, 2, 2, 0, name='pysdl3 sensor pad',
        touchpads=[2], sensors=[(sdl.SENSOR_GYRO, 200.0), sdl.SENSOR_ACCEL],
        set_sensors_enabled=lambda enabled: calls.append(('sensors', enabled)))
    gp = sdl.Gamepad(jid)
    js = gp.GetJoystick()
    _drain(sdl)
    yield jid, gp, js
    js.Close()
    gp.Close()
    sdl.JoystickDetachVirtual(jid)


def test_virtual_touchpad(sdl, sensor_pad):
    jid, gp, js = sensor_pad
    assert gp.GetNumTouchpads() == 1
    assert gp.GetNumTouchpadFingers(0) == 2
    assert gp.GetTouchpadFinger(0, 1) == (False, 0.0, 0.0, 0.0)

    js.SetVirtualTouchpad(0, 1, True, 0.25, 0.5, 1.0)
    sdl.UpdateJoysticks()
    assert gp.GetTouchpadFinger(0, 1) == (True, 0.25, 0.5, 1.0)
    assert (sdl.EVENT_GAMEPAD_TOUCHPAD_DOWN, (jid, 0, 1, 0.25, 0.5, 1.0)) in _drain(sdl)

    js.SetVirtualTouchpad(0, 1, True, 0.75, 0.5, 0.5)
    sdl.UpdateJoysticks()
    assert (sdl.EVENT_GAMEPAD_TOUCHPAD_MOTION, (jid, 0, 1, 0.75, 0.5, 0.5)) in _drain(sdl)

    js.SetVirtualTouchpad(0, 1, False, 0.75, 0.5, 0.0)
    sdl.UpdateJoysticks()
    kinds = [kind for kind, _ in _drain(sdl)]
    assert sdl.EVENT_GAMEPAD_TOUCHPAD_UP in kinds
    assert sdl.EVENT_GAMEPAD_UPDATE_COMPLETE in kinds  # -> (which,)

    with pytest.raises(sdl.error):
        js.SetVirtualTouchpad(0, 2, True, 0, 0, 0)


def test_virtual_sensors(sdl, sensor_pad, calls):
    jid, gp, js = sensor_pad
    assert gp.HasSensor(sdl.SENSOR_GYRO) and gp.HasSensor(sdl.SENSOR_ACCEL)
    assert gp.GetSensorDataRate(sdl.SENSOR_GYRO) == 200.0
    assert gp.IsSensorEnabled(sdl.SENSOR_GYRO) is False

    gp.SetSensorEnabled(sdl.SENSOR_GYRO, True)
    assert gp.IsSensorEnabled(sdl.SENSOR_GYRO) is True
    assert calls[-1] == ('sensors', True)

    if not _sensor_data_supported(sdl):
        with pytest.raises(sdl.error):
            js.SendVirtualSensorData(sdl.SENSOR_GYRO, (1.0, 2.0, 3.0))
        return
    js.SendVirtualSensorData(sdl.SENSOR_GYRO, (1.0, 2.0, 3.0), 1000)
    js.SendVirtualSensorData(sdl.SENSOR_GYRO, [4.0, 5.0, 6.0], sensor_timestamp=2000)
    sdl.UpdateJoysticks()
    updates = [data for kind, data in _drain(sdl) if kind == sdl.EVENT_GAMEPAD_SENSOR_UPDATE]
    assert updates == [(jid, sdl.SENSOR_GYRO, (1.0, 2.0, 3.0), 1000),
                       (jid, sdl.SENSOR_GYRO, (4.0, 5.0, 6.0), 2000)]
    assert gp.GetSensorData(sdl.SENSOR_GYRO) == (4.0, 5.0, 6.0)
    assert gp.GetSensorData(sdl.SENSOR_GYRO, count=5) == (4.0, 5.0, 6.0, 0.0, 0.0)

    with pytest.raises(ValueError):
        js.SendVirtualSensorData(sdl.SENSOR_GYRO, ())
    with pytest.raises(ValueError):
        js.SendVirtualSensorData(sdl.SENSOR_GYRO, [0.0] * 17)


def test_controller_lookup(sdl, vcontroller):
    iid = vcontroller.GetJoystick().InstanceID()
    found = sdl.GameControllerFromInstanceID(iid)
    assert found.Name() == vcontroller.Name()
    found.Close()  # owned reference: the original stays open
    assert vcontroller.Attached() is True

    vcontroller.SetPlayerIndex(1)
    by_player = sdl.GetGamepadFromPlayerIndex(1)
    assert by_player.Name() == vcontroller.Name()
    by_player.Close()
    assert vcontroller.Attached() is True
    assert sdl.GameControllerFromInstanceID(0x7FFFFFF0) is None
    assert iid in sdl.GetGamepads()


def test_controller_mapping_queries(sdl, vpad, vcontroller):
    count = sdl.GameControllerNumMappings()
    assert count > 0
    assert len(sdl.GetGamepadMappings()) == count
    assert isinstance(sdl.GameControllerMappingForIndex(0), str)
    assert sdl.GameControllerMappingForIndex(10 ** 6) is None

    guid = vcontroller.GetJoystick().GetGUID()
    assert sdl.GameControllerMappingForGUID(guid).startswith(guid)
    assert sdl.GetGamepadMappingForGUID(sdl.JoystickGetGUIDFromString(guid)).startswith(guid)
    assert 'a:b0' in sdl.GameControllerMappingForDeviceIndex(vpad)
    assert sdl.GameControllerTypeForIndex(vpad) == vcontroller.GetType()
    assert sdl.GameControllerPathForIndex(vpad) is None


def test_controller_string_names(sdl):
    south = sdl.GAMEPAD_BUTTON_SOUTH
    leftx = sdl.GAMEPAD_AXIS_LEFTX
    assert sdl.GameControllerGetStringForButton(south) == 'a'
    assert sdl.GameControllerGetButtonFromString('a') == south
    assert sdl.GetGamepadStringForAxis(leftx) == 'leftx'
    assert sdl.GetGamepadAxisFromString('leftx') == leftx
    assert sdl.GameControllerGetAxisFromString('nope') == sdl.GAMEPAD_AXIS_INVALID
    assert sdl.GameControllerGetStringForButton(999) is None
    assert sdl.GetGamepadStringForType(sdl.GAMEPAD_TYPE_PS5) == 'ps5'
    assert sdl.GetGamepadTypeFromString('ps5') == sdl.GAMEPAD_TYPE_PS5


def test_add_mappings_from_bytes(sdl):
    sdl.InitSubSystem(sdl.INIT_GAMEPAD)
    # File mappings only apply when their platform matches the running OS.
    data = ('03000000ab0000000000000000000000,pysdl3 bytes pad,a:b0,b:b1,'
            'platform:%s,\n' % sdl.GetPlatform()).encode()
    assert sdl.GameControllerAddMappingsFromFile(data) in (0, 1)
    assert sdl.GameControllerMappingForGUID('03000000ab0000000000000000000000') is not None
    assert sdl.AddGamepadMappingsFromFile(bytearray(data)) in (0, 1)
    with pytest.raises(TypeError):
        sdl.AddGamepadMappingsFromFile(42)


def test_new_event_constants(sdl):
    for name in ('EVENT_GAMEPAD_TOUCHPAD_DOWN', 'EVENT_GAMEPAD_TOUCHPAD_MOTION',
                 'EVENT_GAMEPAD_TOUCHPAD_UP', 'EVENT_GAMEPAD_SENSOR_UPDATE',
                 'EVENT_JOYSTICK_BATTERY_UPDATED', 'EVENT_GAMEPAD_STEAM_HANDLE_UPDATED',
                 'GAMEPAD_BINDTYPE_HAT', 'JOYSTICK_AXIS_MAX', 'JOYSTICK_CONNECTION_WIRED'):
        assert isinstance(getattr(sdl, name), int)
