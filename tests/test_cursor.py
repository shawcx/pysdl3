'''Phase 3: SDL3.Cursor (phase 13: cursor lifetimes). The dummy video driver
has no system cursors, but SDL3 makes colour / bitmap cursors (and a default
cursor) without driver support, so most of these run headless.'''

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


# --- phase 13: cursor lifetime ----------------------------------------------

def _color_cursor(sdl):
    try:
        return sdl.CreateColorCursor(sdl.CreateRGBSurface((8, 8)), 0, 0)
    except sdl.error:
        pytest.skip('cursors unsupported under this driver')


def test_get_cursor_returns_the_owning_object(sdl):
    cursor = _color_cursor(sdl)
    cursor.Set()
    assert sdl.GetCursor() is cursor
    sdl.SetCursor(None)  # SDL's "redraw": the current cursor stays
    assert sdl.GetCursor() is cursor
    default = sdl.GetDefaultCursor()
    if default is not None:
        default.Set()
        assert sdl.GetCursor() is not cursor


def test_freeing_the_current_cursor(sdl):
    # Freeing the current cursor makes SDL fall back to its default one, which
    # GetCursor() then reports as a borrowed wrapper (None if there is none).
    import gc
    cursor = _color_cursor(sdl)
    cursor.Set()
    del cursor
    gc.collect()
    current = sdl.GetCursor()
    assert current is None or type(current).__name__ == 'Cursor'
    if current is not None:
        current.Set()  # SDL's default: still valid


def test_invalid_cursor_is_rejected(sdl):
    zombie = sdl.Cursor()  # an uninitialised Cursor stands in for a freed one
    with pytest.raises(sdl.error, match='not valid'):
        sdl.SetCursor(zombie)  # SDL_SetCursor(NULL) would mean "redraw", not an error
    with pytest.raises(sdl.error, match='not valid'):
        zombie.Set()


def test_surface_argument_is_checked(sdl):
    with pytest.raises(TypeError):
        sdl.CreateColorCursor(object())
    with pytest.raises(sdl.error):
        sdl.CreateColorCursor(type(sdl.CreateRGBSurface((1, 1)))(), 0, 0)  # wraps no surface


def test_quit_invalidates_cursors(sdl):
    # SDL frees every cursor when video shuts down, ones we own included: they
    # must neither be used nor freed a second time (a new cursor can reuse the
    # address), and SDL's default cursor goes too. Run in a subprocess (it
    # tears SDL down).
    import os
    import subprocess
    import sys
    import textwrap
    code = textwrap.dedent('''
        import gc, SDL3

        def raises(fn):
            try:
                fn()
            except SDL3.error:
                return True
            return False

        SDL3.Init(SDL3.INIT_VIDEO)
        try:
            cursor = SDL3.CreateColorCursor(SDL3.CreateRGBSurface((8, 8)), 0, 0)
        except SDL3.error:
            print("skip"); raise SystemExit
        other = SDL3.CreateCursor(bytes(8), bytes(8), (8, 8), (0, 0))
        cursor.Set()
        assert SDL3.GetCursor() is cursor
        default = SDL3.GetDefaultCursor()
        SDL3.QuitSubSystem(SDL3.INIT_VIDEO)
        for c in (cursor, other, default):
            if c is not None:
                assert raises(c.Set) and raises(lambda: SDL3.SetCursor(c))
        assert raises(lambda: SDL3.CreateColorCursor(SDL3.CreateRGBSurface((8, 8)), 0, 0))
        assert raises(lambda: SDL3.CreateCursor(bytes(8), bytes(8), (8, 8), (0, 0)))
        assert SDL3.GetCursor() is None

        SDL3.Init(SDL3.INIT_VIDEO)
        fresh = [SDL3.CreateColorCursor(SDL3.CreateRGBSurface((8, 8)), 0, 0) for _ in range(4)]
        fresh[0].Set()
        del cursor, other, default
        gc.collect()  # the dead wrappers free nothing
        for c in fresh:
            c.Set()
        assert SDL3.GetCursor() is fresh[-1]
        SDL3.Quit()
        print("ok")
    ''')
    env = dict(os.environ, PYTHONPATH=os.path.dirname(sdl.__file__))
    out = subprocess.run([sys.executable, '-c', code], env=env, capture_output=True, text=True)
    if out.stdout.strip() == 'skip':
        pytest.skip('cursors unsupported under this driver')
    assert out.returncode == 0 and out.stdout.strip() == 'ok', out.stderr or out.stdout
