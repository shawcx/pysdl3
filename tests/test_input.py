'''Keyboard, text input, mouse module functions (SDL3).'''

import pytest


# --- keyboard ---------------------------------------------------------------

def test_key_name_and_code_roundtrip(sdl):
    assert sdl.GetKeyFromName('Return') == sdl.K_RETURN
    assert sdl.GetKeyName(sdl.K_RETURN) == 'Return'
    assert sdl.GetScancodeFromName('A') == sdl.SCANCODE_A
    assert sdl.GetScancodeName(sdl.SCANCODE_SPACE) == 'Space'


def test_key_scancode_conversion(sdl):
    # SDL3 keeps only the uppercase SDLK_A name (value is lowercase 'a').
    assert sdl.GetKeyFromScancode(sdl.SCANCODE_A) == sdl.K_A
    assert sdl.GetScancodeFromKey(sdl.K_A) == sdl.SCANCODE_A


def test_unknown_names(sdl):
    assert sdl.GetKeyFromName('definitely not a key') == sdl.K_UNKNOWN
    assert sdl.GetScancodeFromName('definitely not a key') == sdl.SCANCODE_UNKNOWN


def test_mod_state_roundtrip(sdl):
    sdl.SetModState(sdl.KMOD_LSHIFT | sdl.KMOD_LCTRL)
    state = sdl.GetModState()
    assert state & sdl.KMOD_LSHIFT
    assert state & sdl.KMOD_LCTRL
    sdl.SetModState(sdl.KMOD_NONE)
    assert sdl.GetModState() == sdl.KMOD_NONE


def test_keyboard_focus_and_keystate(sdl):
    focus = sdl.GetKeyboardFocus()
    assert focus is None or type(focus).__name__ == 'Window'
    assert isinstance(sdl.HasScreenKeyboardSupport(), bool)

    keys = sdl.GetKeyState()
    assert isinstance(keys, list) and len(keys) > 100
    assert all(isinstance(k, bool) for k in keys[:10])


# --- text input (SDL3: per-window) --------------------------------------------

def test_text_input_lifecycle(sdl, window):
    sdl.StartTextInput(window)
    assert sdl.IsTextInputActive(window) is True
    sdl.SetTextInputArea(window, (0, 0, 100, 20))
    sdl.SetTextInputArea(window, None)
    sdl.StopTextInput(window)
    assert sdl.IsTextInputActive(window) is False


# --- mouse (SDL3: float coordinates) -----------------------------------------

def test_mouse_state_shapes(sdl):
    for getter in (sdl.GetMouseState, sdl.GetGlobalMouseState, sdl.GetRelativeMouseState):
        buttons, x, y = getter()
        assert isinstance(buttons, int)
        assert isinstance(x, float) and isinstance(y, float)


def test_warp_and_relative_mode(sdl, window):
    sdl.WarpMouseInWindow(window, 10, 10)
    sdl.WarpMouseInWindow(None, 5, 5)

    try:
        sdl.SetRelativeMouseMode(window, True)
    except sdl.error:
        pytest.skip('relative mouse mode unsupported under this driver')
    assert sdl.GetRelativeMouseMode(window) is True
    sdl.SetRelativeMouseMode(window, False)
    assert sdl.GetRelativeMouseMode(window) is False


def test_warp_rejects_non_window(sdl):
    with pytest.raises(TypeError):
        sdl.WarpMouseInWindow(object(), 0, 0)


def test_mouse_focus_is_borrowed(sdl, window):
    # A borrowed Window wrapper must not destroy the window it points at.
    focus = sdl.GetMouseFocus()
    if focus is not None:
        assert type(focus).__name__ == 'Window'
        del focus  # dealloc of a borrowed window: no crash, window still usable
    assert window.GetWindowSize() == (64, 48)


# --- phase 13: IME composition / keyboard reset ------------------------------

def test_clear_composition(sdl, window):
    sdl.StartTextInput(window)
    try:
        sdl.ClearComposition(window)
        assert sdl.TextInputActive(window) is True  # SDL3 name of IsTextInputActive
    finally:
        sdl.StopTextInput(window)
    with pytest.raises(TypeError):
        sdl.ClearComposition(None)
    assert sdl.ScreenKeyboardShown(window) in (True, False)


def test_reset_keyboard(sdl):
    sdl.ResetKeyboard()
    assert not any(sdl.GetKeyState())
    assert sdl.GetModState() == sdl.KMOD_NONE
