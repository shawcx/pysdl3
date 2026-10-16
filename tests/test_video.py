'''Window state, display queries, hints, message boxes, GL/Vulkan (SDL3).

The dummy video driver rejects a lot of window operations ("That operation is
not supported"); those are exercised for their argument handling and skipped
when the driver refuses.'''

import pytest


def _try(fn):
    '''Run fn(); return its value, or None if the driver does not support it.'''
    try:
        return fn()
    except Exception as exc:  # noqa: BLE001 - SDL3.error, matched below
        if 'not supported' in str(exc) or 'No ' in str(exc):
            pytest.skip(str(exc))
        raise


# --- window ----------------------------------------------------------------

def test_window_flags_and_format(window, sdl):
    assert isinstance(window.GetWindowFlags(), int)
    assert window.GetWindowDisplayIndex() > 0        # SDL3 SDL_DisplayID
    assert isinstance(window.GetWindowPixelFormat(), int)


def test_window_min_max_size(window):
    window.SetWindowMinimumSize((80, 60))
    window.SetWindowMaximumSize((800, 600))
    assert window.GetWindowMinimumSize() == (80, 60)
    assert window.GetWindowMaximumSize() == (800, 600)


def test_window_grab_roundtrip(window):
    window.SetWindowGrab(True)
    assert window.GetWindowGrab() is True
    window.SetWindowGrab(False)
    assert window.GetWindowGrab() is False


def test_window_bordered_and_modal(window, sdl):
    window.SetWindowBordered(False)
    window.SetWindowBordered(True)
    with pytest.raises(TypeError):
        window.SetWindowModalFor(object())


def test_window_opacity(window):
    _try(lambda: window.SetWindowOpacity(0.5))
    assert 0.0 <= window.GetWindowOpacity() <= 1.0


def test_window_display_mode(window):
    # SDL3: the fullscreen mode is None until one is requested.
    mode = window.GetWindowDisplayMode()
    assert mode is None or (mode[1] > 0 and mode[2] > 0)


def test_window_mouse_rect(window):
    _try(lambda: window.SetWindowMouseRect((0, 0, 40, 40)))
    assert window.GetWindowMouseRect() == (0, 0, 40, 40)
    window.SetWindowMouseRect(None)
    assert window.GetWindowMouseRect() is None


# --- displays -------------------------------------------------------------

def test_display_queries(sdl):
    display = sdl.GetDisplays()[0]
    assert isinstance(sdl.GetDisplayName(display), str)
    x, y, w, h = sdl.GetDisplayUsableBounds(display)
    assert w > 0 and h > 0
    assert sdl.GetDisplayOrientation(display) in (
        sdl.ORIENTATION_UNKNOWN, sdl.ORIENTATION_LANDSCAPE, sdl.ORIENTATION_LANDSCAPE_FLIPPED,
        sdl.ORIENTATION_PORTRAIT, sdl.ORIENTATION_PORTRAIT_FLIPPED,
    )

    n = sdl.GetNumDisplayModes(display)
    assert n >= 0
    if n:
        fmt, mw, mh, rate = sdl.GetDisplayMode(display, 0)
        assert mw > 0 and mh > 0

    closest = sdl.GetClosestDisplayMode(display, (0, 640, 480, 60))
    assert closest is None or len(closest) == 4


def test_get_window_from_id_is_borrowed(sdl, window):
    fetched = sdl.GetWindowFromID(window.GetWindowID())
    assert fetched.GetWindowID() == window.GetWindowID()
    del fetched  # borrowed: window must survive
    assert window.GetWindowSize() == (64, 48)

    assert sdl.GetWindowFromID(999999) is None


# --- hints --------------------------------------------------------------------

def test_hints(sdl):
    assert sdl.SetHint('SDL_pysdl3_test', 'yes') is True
    assert sdl.GetHint('SDL_pysdl3_test') == 'yes'
    assert sdl.GetHintBoolean('SDL_pysdl3_test', False) is True
    assert sdl.GetHint('SDL_pysdl3_never_set') is None

    sdl.SetHintWithPriority('SDL_pysdl3_test', 'no', sdl.HINT_OVERRIDE)
    assert sdl.GetHint('SDL_pysdl3_test') == 'no'
    sdl.ClearHints()


# --- message boxes ----------------------------------------------------------

def test_show_message_box_rejects_bad_config(sdl):
    with pytest.raises(TypeError):
        sdl.ShowMessageBox([1, 2, 3])
    with pytest.raises(KeyError):
        sdl.ShowMessageBox({'title': 'x', 'message': 'y'})  # no buttons


# --- locales / GL / Vulkan ------------------------------------------------

def test_preferred_locales(sdl):
    locales = sdl.GetPreferredLocales()
    assert isinstance(locales, list)
    for lang, country in locales:
        assert isinstance(lang, str)
        assert country is None or isinstance(country, str)


def test_gl_get_proc_address(sdl):
    assert isinstance(sdl.GL_GetProcAddress('glClear'), int)
    assert isinstance(sdl.GL_GetCurrentContext(), int)


def test_window_flag_constants_present(sdl):
    for name in ('WINDOW_ALWAYS_ON_TOP', 'WINDOW_UTILITY', 'WINDOW_POPUP_MENU',
                 'WINDOW_VULKAN', 'BLENDMODE_MUL', 'EVENT_LOCALE_CHANGED'):
        assert isinstance(getattr(sdl, name), int), name


def test_metal_view(sdl):
    try:
        win = sdl.Window('metal-test', (32, 32), flags=sdl.WINDOW_METAL)
        view = win.Metal_CreateView()
    except sdl.error:
        pytest.skip('no Metal support in this driver / platform')
    assert isinstance(view, int) and view != 0
    assert isinstance(sdl.Metal_GetLayer(view), int)
    assert win.Metal_GetDrawableSize() == (32, 32)
    sdl.Metal_DestroyView(view)


def test_vulkan_functions_present(sdl, window):
    assert hasattr(sdl, 'Vulkan_GetInstanceExtensions')
    assert hasattr(window, 'Vulkan_CreateSurface')
    try:
        sdl.Vulkan_LoadLibrary()
    except sdl.error:
        pytest.skip('no Vulkan loader available')
    try:
        extensions = sdl.Vulkan_GetInstanceExtensions()
        assert isinstance(extensions, list)
        assert all(isinstance(e, str) for e in extensions)
        assert sdl.Vulkan_GetVkGetInstanceProcAddr() != 0
        with pytest.raises(TypeError):
            window.Vulkan_CreateSurface('not an int')
    finally:
        sdl.Vulkan_UnloadLibrary()


# --- window-surface tracking and teardown invalidation ---------------------

def test_window_surface_is_one_object(sdl):
    window = sdl.Window('surface-identity', (32, 24))
    surface = window.GetWindowSurface()
    assert window.GetWindowSurface() is surface
    borrowed = sdl.GetWindowFromID(window.GetWindowID())
    assert borrowed.GetWindowSurface() is surface  # shared across wrappers


def test_window_surface_replaced_after_resize(sdl):
    window = sdl.Window('surface-resize', (32, 24), flags=sdl.WINDOW_RESIZABLE)
    old = window.GetWindowSurface()
    window.SetWindowSize((64, 48))
    sdl.PumpEvents()
    assert (old.w, old.h) == (32, 24)  # SDL keeps it until the next GetWindowSurface
    new = window.GetWindowSurface()
    assert new is not old and (new.w, new.h) == (64, 48)
    assert (old.w, old.h) == (0, 0)    # emptied, not dangling
    old.FillRect(None, (1, 2, 3))      # harmless
    new.FillRect(None, (1, 2, 3))
    window.UpdateWindowSurface()


def test_window_surface_emptied_on_destroy(sdl):
    window = sdl.Window('surface-destroy', (16, 16))
    surface = window.GetWindowSurface()
    del window  # SDL frees the surface with the window
    assert (surface.w, surface.h) == (0, 0)
    surface.FillRect(None, (1, 2, 3))


def test_borrowed_window_invalidated_with_owner(sdl):
    window = sdl.Window('owner', (16, 16))
    borrowed = sdl.GetWindowFromID(window.GetWindowID())
    del window
    with pytest.raises(sdl.error):
        borrowed.GetWindowID()


def test_destroying_parent_invalidates_children(sdl):
    # SDL_DestroyWindow takes child windows (SetWindowModalFor parents them)
    # down too, with their surfaces, renderers and textures. (The dummy driver
    # has no parent windows: this runs on a real one, e.g. SDL_VIDEODRIVER=cocoa.)
    parent = sdl.Window('parent', (32, 32), flags=sdl.WINDOW_HIDDEN)
    child = sdl.Window('child', (16, 16), flags=sdl.WINDOW_HIDDEN)
    _try(lambda: child.SetWindowModalFor(parent))
    renderer = child.CreateRenderer()
    texture = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA32, sdl.TEXTUREACCESS_STATIC, (2, 2))
    borrowed = sdl.GetWindowFromID(child.GetWindowID())
    del parent
    for fn in (child.GetWindowID, borrowed.GetWindowID, renderer.Clear, texture.Query):
        with pytest.raises(sdl.error):
            fn()


def _run_teardown_script(sdl, body):
    # Quit tears down the session the `sdl` fixture shares: use a subprocess.
    import os
    import subprocess
    import sys
    import textwrap
    code = textwrap.dedent('''
        import gc
        import SDL3

        def raises(fn):
            try:
                fn()
            except SDL3.error:
                return True
            return False

        SDL3.Init(SDL3.INIT_VIDEO)
    ''') + textwrap.dedent(body) + '\nprint("ok")\n'
    env = dict(os.environ, PYTHONPATH=os.path.dirname(sdl.__file__))
    out = subprocess.run([sys.executable, '-c', code], env=env, capture_output=True, text=True)
    assert out.returncode == 0 and out.stdout.strip().endswith('ok'), out.stderr or out.stdout


def test_quit_invalidates_windows(sdl):
    _run_teardown_script(sdl, '''
        window = SDL3.Window('w', (32, 32))
        surface = window.GetWindowSurface()
        borrowed = SDL3.GetWindowFromID(window.GetWindowID())

        SDL3.Quit()
        assert raises(window.GetWindowID) and raises(borrowed.GetWindowID)
        assert (surface.w, surface.h) == (0, 0)

        # A new session can reuse window ids and addresses; old wrappers stay dead.
        SDL3.Init(SDL3.INIT_VIDEO)
        fresh = SDL3.Window('fresh', (16, 16))
        assert raises(window.GetWindowID) and raises(borrowed.GetWindowID)
        del window, borrowed, surface
        gc.collect()
        assert fresh.GetWindowID() > 0  # dropping the old wrappers didn't destroy it
        SDL3.Quit()
    ''')


def test_quit_subsystem_respects_refcount(sdl):
    _run_teardown_script(sdl, '''
        window = SDL3.Window('w', (16, 16))
        SDL3.InitSubSystem(SDL3.INIT_VIDEO)   # second reference
        SDL3.QuitSubSystem(SDL3.INIT_VIDEO)   # video keeps running
        assert window.GetWindowID() > 0
        SDL3.QuitSubSystem(SDL3.INIT_VIDEO)   # now it stops
        assert raises(window.GetWindowID)
        del window
        gc.collect()
        SDL3.Quit()
    ''')


# --- phase 12: pixel size, ICC profile, window surface, shape --------------

def test_window_size_in_pixels(sdl, window):
    assert window.GetWindowSizeInPixels() == window.GetWindowSize()  # no high-DPI here


def test_window_icc_profile(sdl, window):
    profile = window.GetWindowICCProfile()
    assert profile is None or isinstance(profile, bytes)


def test_window_surface_lifecycle(sdl):
    window = sdl.Window('surface', (16, 16))
    assert window.HasWindowSurface() is False
    surface = window.GetWindowSurface()
    assert window.WindowHasSurface() is True
    window.DestroyWindowSurface()
    assert window.HasWindowSurface() is False
    assert (surface.w, surface.h) == (0, 0)  # emptied first, not dangling
    surface.FillRect(None, (1, 2, 3))
    assert window.GetWindowSurface().w == 16


def test_set_window_shape(sdl):
    window = sdl.Window('shaped', (16, 16), flags=sdl.WINDOW_TRANSPARENT)
    shape = sdl.CreateSurface((16, 16), sdl.PIXELFORMAT_RGBA32)
    shape.FillRect((4, 4, 8, 8), (255, 255, 255, 255))
    _try(lambda: window.SetWindowShape(shape))
    if sdl.GetVersion() >= (3, 4, 4):
        window.SetWindowShape(None)
    else:
        with pytest.raises(sdl.error, match='3.4.4'):
            window.SetWindowShape(None)  # SDL before 3.4.4 can't remove a shape
    with pytest.raises(TypeError):
        window.SetWindowShape(object())
    with pytest.raises(sdl.error):
        window.SetWindowShape(type(shape)())  # uninitialised: not "no shape"


def test_quit_invalidates_renderers_and_textures(sdl):
    _run_teardown_script(sdl, '''
        window = SDL3.Window('w', (32, 32))
        renderer = window.CreateRenderer()
        texture = SDL3.Texture(renderer, SDL3.PIXELFORMAT_RGBA32, SDL3.TEXTUREACCESS_STREAMING, (4, 4))
        locked = texture.LockToSurface()
        software = SDL3.CreateSoftwareRenderer(SDL3.CreateSurface((8, 8), SDL3.PIXELFORMAT_RGBA32))
        soft_texture = SDL3.Texture(software, size=(2, 2))

        SDL3.Quit()  # SDL_QuitRender freed every renderer and texture
        assert raises(renderer.Clear) and raises(renderer.RenderFlush) and raises(software.Clear)
        assert raises(texture.Query) and raises(soft_texture.Query)
        assert (locked.w, locked.h) == (0, 0)

        # New objects may reuse the addresses; the old wrappers stay dead.
        SDL3.Init(SDL3.INIT_VIDEO)
        fresh_window = SDL3.Window('fresh', (16, 16))
        fresh = fresh_window.CreateRenderer()
        fresh_texture = SDL3.Texture(fresh, size=(2, 2))
        del window, renderer, texture, locked, software, soft_texture
        gc.collect()
        assert fresh_texture.Query()[2:] == (2, 2)
        fresh.Clear()
        SDL3.Quit()
    ''')


def test_quit_subsystem_without_video_keeps_software_renderer(sdl):
    # A software renderer on a Surface needs no video; only shutting video down
    # (which runs SDL_QuitRender) may invalidate it.
    _run_teardown_script(sdl, '''
        SDL3.QuitSubSystem(SDL3.INIT_VIDEO)
        software = SDL3.CreateSoftwareRenderer(SDL3.CreateSurface((8, 8), SDL3.PIXELFORMAT_RGBA32))
        texture = SDL3.Texture(software, size=(2, 2))
        SDL3.InitSubSystem(SDL3.INIT_EVENTS)
        SDL3.QuitSubSystem(SDL3.INIT_EVENTS)
        software.Clear()
        assert texture.Query()[2:] == (2, 2)
        SDL3.Quit()
        assert raises(software.Clear) and raises(texture.Query)
    ''')
