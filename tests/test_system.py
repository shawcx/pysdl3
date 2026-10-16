'''Phase 6: power, CPU extras, touch, and the haptic / sensor device wrappers.

No haptic hardware or sensors exist under the headless drivers, so these mostly
check the query surface and the uninitialised-instance guards.'''

import pytest


def test_power_info(sdl):
    state, seconds, percent = sdl.GetPowerInfo()
    assert state in (
        sdl.POWERSTATE_UNKNOWN, sdl.POWERSTATE_ON_BATTERY, sdl.POWERSTATE_NO_BATTERY,
        sdl.POWERSTATE_CHARGING, sdl.POWERSTATE_CHARGED,
    )
    assert isinstance(seconds, int)
    assert isinstance(percent, int)


def test_cpu_extras(sdl):
    assert sdl.GetSystemRAM() > 0
    assert isinstance(sdl.HasNEON(), bool)
    assert isinstance(sdl.HasAVX512F(), bool)
    if hasattr(sdl, 'SIMDGetAlignment'):
        assert sdl.SIMDGetAlignment() >= 1


def test_touch_query(sdl):
    n = sdl.GetNumTouchDevices()
    assert n >= 0
    for i in range(n):
        tid = sdl.GetTouchDevice(i)
        assert sdl.GetNumTouchFingers(tid) >= 0
        assert sdl.GetTouchDeviceType(tid) in (
            sdl.TOUCH_DEVICE_INVALID, sdl.TOUCH_DEVICE_DIRECT,
            sdl.TOUCH_DEVICE_INDIRECT_ABSOLUTE, sdl.TOUCH_DEVICE_INDIRECT_RELATIVE,
        )


def test_touch_finger_out_of_range_is_none(sdl):
    assert sdl.GetTouchFinger(sdl.MOUSE_TOUCHID, 999) is None


# --- haptic -----------------------------------------------------------------

def test_haptic_module_functions(sdl):
    assert isinstance(sdl.NumHaptics(), int)
    assert isinstance(sdl.MouseIsHaptic(), bool)


def test_uninitialised_haptic_is_guarded(sdl):
    with pytest.raises(sdl.error):
        sdl.Haptic().Query()


def test_haptic_open_bad_index_raises(sdl):
    with pytest.raises(sdl.error):
        sdl.Haptic(99)


def test_joystick_is_haptic_type_check(sdl):
    with pytest.raises(TypeError):
        sdl.JoystickIsHaptic(object())


# --- sensor ---------------------------------------------------------------

def test_sensor_module_functions(sdl):
    assert sdl.NumSensors() >= 0
    for i in range(sdl.NumSensors()):
        assert isinstance(sdl.SensorGetDeviceName(i), str)
        assert sdl.SensorGetDeviceType(i) in (
            sdl.SENSOR_INVALID, sdl.SENSOR_UNKNOWN, sdl.SENSOR_ACCEL, sdl.SENSOR_GYRO,
        )
    sdl.SensorUpdate()


def test_uninitialised_sensor_is_guarded(sdl):
    with pytest.raises(sdl.error):
        sdl.Sensor().GetData()


def test_sensor_open_bad_index_raises(sdl):
    with pytest.raises(sdl.error):
        sdl.Sensor(99)


# --- phase 13: primary selection, sensor / haptic lookups, misc --------------

def test_primary_selection(sdl):
    # Where the video driver has no primary selection (dummy, Cocoa) SDL keeps
    # the text itself, so it still round-trips.
    try:
        sdl.SetPrimarySelectionText('pysdl3 primary')
    except sdl.error:
        pytest.skip('no primary selection under this video driver')
    assert sdl.HasPrimarySelectionText() is True
    assert sdl.GetPrimarySelectionText() == 'pysdl3 primary'
    sdl.SetPrimarySelectionText('')
    assert sdl.HasPrimarySelectionText() is False


def test_sensor_lookups_without_devices(sdl):
    sdl.InitSubSystem(sdl.INIT_SENSOR)
    try:
        assert sdl.SensorFromInstanceID(12345) is None
        assert sdl.GetSensorFromID(12345) is None
        assert sdl.SensorGetDeviceNonPortableType(12345) == -1
        assert sdl.GetSensorNonPortableTypeForID(12345) == -1
        assert sdl.GetSensorTypeForID(12345) == sdl.SENSOR_INVALID
        for sensor_id in sdl.GetSensors():
            assert isinstance(sdl.GetSensorNonPortableTypeForID(sensor_id), int)
    finally:
        sdl.QuitSubSystem(sdl.INIT_SENSOR)


def test_haptic_lookups(sdl):
    sdl.InitSubSystem(sdl.INIT_HAPTIC)
    try:
        for haptic_id in sdl.GetHaptics():
            assert isinstance(sdl.HapticOpened(haptic_id), bool)
        assert sdl.HapticOpened(999999) is False
        assert sdl.GetHapticFromID(999999) is None
    finally:
        sdl.QuitSubSystem(sdl.INIT_HAPTIC)


def test_error_msg_alias(sdl):
    sdl.SetError('pysdl3 test error')
    assert sdl.GetErrorMsg() == sdl.GetError() == 'pysdl3 test error'
    sdl.ClearError()
    assert not hasattr(sdl, 'HasRDTSC')  # SDL3 dropped SDL_HasRDTSC
