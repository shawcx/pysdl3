'''Phase 3: SDL2.Cursor. The dummy video driver has no cursor support, so most
of these skip there; they still exercise the argument checking and error paths.'''

import pytest


@pytest.fixture
def system_cursor(sdl):
    try:
        return sdl.Cursor(sdl.SYSTEM_CURSOR_POINTER)
    except sdl.error:
        pytest.skip('cursors unsupported under this driver')


def test_system_cursor_set(system_cursor):
    system_cursor.Set()


def test_uninitialised_cursor_set_is_guarded(sdl):
    with pytest.raises(sdl.error):
        sdl.Cursor().Set()


def test_color_cursor(sdl):
    surface = sdl.CreateRGBSurface((16, 16))
    surface.FillRect(None, (255, 255, 255, 255))
    try:
        cursor = sdl.CreateColorCursor(surface, 8, 8)
    except sdl.error:
        pytest.skip('cursors unsupported under this driver')
    cursor.Set()

    with pytest.raises(TypeError):
        sdl.CreateColorCursor(object())


def test_create_cursor_validates_buffer_size(sdl):
    with pytest.raises(ValueError):
        sdl.CreateCursor(b'\x00', b'\x00', (16, 16), (0, 0))


def test_create_cursor_from_bitmap(sdl):
    data = bytes((16 + 7) // 8 * 16)
    mask = bytes((16 + 7) // 8 * 16)
    try:
        cursor = sdl.CreateCursor(data, mask, (16, 16), (0, 0))
    except sdl.error:
        pytest.skip('cursors unsupported under this driver')
    cursor.Set()


def test_get_cursor_borrowed(sdl):
    default = sdl.GetDefaultCursor()
    active = sdl.GetCursor()
    for cur in (default, active):
        assert cur is None or type(cur).__name__ == 'Cursor'
    # deallocating a borrowed cursor must not free SDL's copy
    del default, active
    sdl.SetCursor(None)


def test_set_cursor_rejects_non_cursor(sdl):
    with pytest.raises(TypeError):
        sdl.SetCursor(object())
