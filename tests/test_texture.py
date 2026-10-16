'''Phase 1: Texture construction, pixel upload, modulation, locking.'''

import pytest


@pytest.fixture
def static_texture(sdl, renderer):
    return sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA8888, sdl.TEXTUREACCESS_STATIC, (4, 4))


def test_constructor_and_query(sdl, renderer):
    tex = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA8888, sdl.TEXTUREACCESS_STATIC, (8, 6))
    fmt, access, w, h = tex.Query()
    assert (w, h) == (8, 6)
    assert access == sdl.TEXTUREACCESS_STATIC


def test_constructor_rejects_non_renderer(sdl):
    with pytest.raises(TypeError):
        sdl.Texture(object())


def test_update_full_and_partial(static_texture):
    static_texture.Update(b'\xff' * (4 * 4 * 4))
    static_texture.Update(b'\x80' * (2 * 2 * 4), (1, 1, 2, 2))
    static_texture.Update(b'\x80' * (2 * 2 * 4), rect=(1, 1, 2, 2), pitch=8)


def test_color_alpha_blend_mod_roundtrip(sdl, static_texture):
    static_texture.SetColorMod(10, 20, 30)
    assert static_texture.GetColorMod() == (10, 20, 30)

    static_texture.SetAlphaMod(128)
    assert static_texture.GetAlphaMod() == 128

    static_texture.SetBlendMode(sdl.BLENDMODE_ADD)
    assert static_texture.GetBlendMode() == sdl.BLENDMODE_ADD


def test_scale_mode_roundtrip(sdl, static_texture):
    if not hasattr(static_texture, 'SetScaleMode'):
        pytest.skip('SDL_*TextureScaleMode needs SDL >= 2.0.12')
    static_texture.SetScaleMode(sdl.SCALEMODE_LINEAR)
    assert static_texture.GetScaleMode() == sdl.SCALEMODE_LINEAR


def test_lock_returns_writable_view(sdl, renderer):
    tex = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA8888, sdl.TEXTUREACCESS_STREAMING, (4, 4))
    view, pitch = tex.Lock()
    try:
        assert pitch >= 4 * 4
        assert len(view) == pitch * 4
        assert view.readonly is False
        view[0] = 123
    finally:
        tex.Unlock()


def test_lock_region(sdl, renderer):
    tex = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA8888, sdl.TEXTUREACCESS_STREAMING, (8, 8))
    view, pitch = tex.Lock((0, 0, 2, 2))
    try:
        # rows are `pitch` apart; the view ends with the 2-pixel last row
        assert len(view) == pitch + 2 * 4
    finally:
        tex.Unlock()


def test_lock_view_stays_inside_the_texture(sdl, renderer):
    # Lock used to return pitch * h bytes from the rect's first pixel, which
    # for a rect touching the bottom edge (or one off the texture: SDL doesn't
    # clip) reached past the texture's pixels.
    tex = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA8888, sdl.TEXTUREACCESS_STREAMING, (8, 8))
    view, pitch = tex.Lock((6, 6, 2, 2))
    try:
        assert len(view) == pitch + 2 * 4
        view[:] = b'\xff' * len(view)
    finally:
        tex.Unlock()
    for rect in ((5000, 5000, 4, 4), (6, 6, 4, 4), (-1, 0, 2, 2), (0, 0, 0, 2)):
        with pytest.raises(ValueError):
            tex.Lock(rect)


def test_update_yuv_checks_plane_sizes(sdl, renderer):
    # UpdateYUV used to hand the planes to SDL unchecked, so a short buffer
    # made SDL read past its end.
    tex = sdl.Texture(renderer, sdl.PIXELFORMAT_IYUV, sdl.TEXTUREACCESS_STREAMING, (4, 4))
    y, u, v = b'\x80' * 16, b'\x80' * 4, b'\x80' * 4  # 4x4 luma, 2x2 chroma
    tex.UpdateYUV(y, 4, u, 2, v, 2)
    tex.UpdateYUV(y[:8], 4, u[:2], 2, v[:2], 2, rect=(0, 0, 4, 2))
    tex.UpdateYUV(y[:8], 4, u[:2], 2, v[:2], 2, rect=(2, 2, 4, 4))  # clipped to 2x2
    tex.UpdateYUV(b'\x80', 4, b'\x80', 2, b'\x80', 2, rect=(8, 8, 4, 4))  # off-texture: nothing read
    with pytest.raises(ValueError):
        tex.UpdateYUV(y[:15], 4, u, 2, v, 2)
    with pytest.raises(ValueError):
        tex.UpdateYUV(y, 4, u, 2, v[:3], 2)
    with pytest.raises(ValueError):
        tex.UpdateYUV(y, 0, u, 2, v, 2)
    with pytest.raises(ValueError):
        tex.UpdateYUV(y, 2, u, 2, v, 2)  # pitch below the 4-byte row


def test_update_yuv_rect_partly_off_texture(sdl, renderer):
    # A rect whose clip is empty in one direction only (x past the texture, y
    # inside) made SDL copy rows of negative width and crash.
    tex = sdl.Texture(renderer, sdl.PIXELFORMAT_IYUV, sdl.TEXTUREACCESS_STREAMING, (4, 4))
    tex.UpdateYUV(b'\x80' * 16, 4, b'\x80' * 4, 2, b'\x80' * 4, 2, rect=(8, 0, 4, 4))


def test_update_checks_buffer_size(sdl, renderer):
    # Update handed SDL the buffer unchecked, so a short one was read past its end.
    tex = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA8888, sdl.TEXTUREACCESS_STATIC, (8, 8))
    tex.Update(b'\0' * 256)
    tex.Update(b'\0' * 64, rect=(6, 6, 4, 4))   # rows of the rect, clipped to 2x2
    tex.Update(b'\0', rect=(100, 100, 4, 4))     # off the texture: nothing read
    for args, kwargs in (((b'x',), {}), ((b'\0' * 255,), {}),
                         ((b'\0' * 256,), {'pitch': 16}),     # pitch shorter than a row
                         ((b'\0' * 16,), {'rect': (0, 0, 4, 4)})):
        with pytest.raises(ValueError):
            tex.Update(*args, **kwargs)
    yuv = sdl.Texture(renderer, sdl.PIXELFORMAT_IYUV, sdl.TEXTUREACCESS_STREAMING, (4, 4))
    yuv.Update(b'\x80' * 24)                     # 4x4 luma + two 2x2 chroma planes
    with pytest.raises(ValueError):
        yuv.Update(b'\x80' * 23)


# --- phase 12: LockToSurface, UpdateNV -------------------------------------

def test_lock_to_surface(sdl, renderer):
    tex = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA32, sdl.TEXTUREACCESS_STREAMING, (8, 4))
    surface = tex.LockToSurface()
    assert (surface.w, surface.h) == (8, 4)
    surface.FillRect(None, (10, 20, 30, 255))
    with pytest.raises(sdl.error):
        tex.LockToSurface()  # one locked surface at a time
    tex.Unlock()

    # SDL freed the locked surface; ours is now an empty stand-in, not dangling.
    assert (surface.w, surface.h) == (0, 0)
    surface.FillRect(None, (0, 0, 0, 255))

    # The upload happened: draw the texture and read it back.
    renderer.Copy(tex, None, (0, 0, 8, 4))
    assert renderer.RenderReadPixels((1, 1, 1, 1), sdl.PIXELFORMAT_RGBA32) == bytes((10, 20, 30, 255))

    part = tex.LockToSurface((6, 2, 4, 4))  # clipped to the texture, as SDL does
    assert (part.w, part.h) == (2, 2)
    del tex  # dealloc while locked also detaches the surface
    assert (part.w, part.h) == (0, 0)


def test_lock_to_surface_checks(sdl, renderer):
    tex = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA32, sdl.TEXTUREACCESS_STATIC, (4, 4))
    with pytest.raises(sdl.error):
        tex.LockToSurface()  # needs TEXTUREACCESS_STREAMING
    tex = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA32, sdl.TEXTUREACCESS_STREAMING, (4, 4))
    with pytest.raises(ValueError):
        tex.LockToSurface((8, 0, 4, 4))  # misses the texture
    surface = tex.LockToSurface()
    try:
        with pytest.raises(sdl.error):
            sdl.CreateSoftwareRenderer(surface)  # SDL frees it on Unlock: can't be a target
    finally:
        tex.Unlock()


def test_update_nv(sdl, renderer):
    tex = sdl.Texture(renderer, sdl.PIXELFORMAT_NV12, sdl.TEXTUREACCESS_STREAMING, (4, 4))
    tex.UpdateNV(b'\x80' * 16, 4, b'\x80' * 8, 4)
    tex.UpdateNV(b'\x80' * 8, 4, b'\x80' * 4, 4, rect=(0, 0, 4, 2))
    tex.UpdateNV(b'\x80' * 8, 4, b'\x80' * 4, 4, rect=(2, 2, 4, 4))  # clipped to 2x2
    tex.UpdateNV(b'\x80', 4, b'\x80', 4, rect=(8, 0, 4, 4))           # off the texture
    with pytest.raises(ValueError):
        tex.UpdateNV(b'\x80' * 15, 4, b'\x80' * 8, 4)
    with pytest.raises(ValueError):
        tex.UpdateNV(b'\x80' * 16, 4, b'\x80' * 7, 4)
    with pytest.raises(ValueError):
        tex.UpdateNV(b'\x80' * 16, 2, b'\x80' * 8, 4)  # Y pitch shorter than a row
    with pytest.raises(ValueError):
        tex.UpdateNV(b'\x80' * 16, 4, b'\x80' * 8, 2)  # UV rows are 4 bytes


# --- renderer / window teardown invalidates textures -----------------------

import gc  # noqa: E402


def _software_renderer(sdl):
    # The renderer keeps its (otherwise unreferenced) Surface alive.
    return sdl.CreateSoftwareRenderer(sdl.CreateRGBSurface((8, 8)))


def test_software_renderer_keeps_its_surface(sdl):
    renderer = _software_renderer(sdl)
    gc.collect()
    renderer.SetRenderDrawColor(1, 2, 3, 255)
    renderer.Clear()
    renderer.Present()
    assert renderer.RenderReadPixels((0, 0, 1, 1), sdl.PIXELFORMAT_RGBA32) == bytes((1, 2, 3, 255))
    assert renderer.RenderGetWindow() is None
    with pytest.raises(sdl.error):
        sdl.CreateSoftwareRenderer(type(sdl.CreateRGBSurface((1, 1)))())  # uninitialised


def test_textures_invalidated_with_renderer(sdl):
    renderer = _software_renderer(sdl)
    made = [
        sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA32, sdl.TEXTUREACCESS_STATIC, (4, 4)),
        renderer.CreateTextureFromSurface(sdl.CreateRGBSurface((4, 4))),
        renderer.LoadTexture(sdl.CreateRGBSurface((4, 4)).SavePNG()),
    ]
    del renderer
    gc.collect()
    for texture in made:
        with pytest.raises(sdl.error):
            texture.Query()
    del made
    gc.collect()  # freeing invalidated textures must not touch the freed ones


def test_invalidated_texture_rejected_by_other_renderer(sdl):
    renderer = _software_renderer(sdl)
    texture = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA32, sdl.TEXTUREACCESS_TARGET, (4, 4))
    other = _software_renderer(sdl)
    survivor = sdl.Texture(other, sdl.PIXELFORMAT_RGBA32, sdl.TEXTUREACCESS_STATIC, (2, 2))
    del renderer
    gc.collect()
    # A NULL texture means "none" to these SDL calls: they must raise instead.
    with pytest.raises(sdl.error, match='Renderer was destroyed'):
        other.SetRenderTarget(texture)
    with pytest.raises(sdl.error, match='Renderer was destroyed'):
        other.RenderGeometry(texture, [((0, 0), (255, 255, 255, 255), (0, 0))])
    with pytest.raises(sdl.error, match='Renderer was destroyed'):
        other.RenderGeometryRaw(texture, b'\0' * 24, b'\0' * 48, b'\0' * 24)
    with pytest.raises(sdl.error):
        other.Copy(texture)
    assert survivor.Query()[2:] == (2, 2)  # other renderers' textures untouched


def test_locked_surface_emptied_with_renderer(sdl):
    renderer = _software_renderer(sdl)
    texture = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA32, sdl.TEXTUREACCESS_STREAMING, (4, 4))
    surface = texture.LockToSurface()
    del renderer
    gc.collect()
    assert (surface.w, surface.h) == (0, 0)
    texture.Unlock()  # harmless


def test_window_get_renderer_is_the_owner(sdl):
    window = sdl.Window('renderer-owner', (16, 16))
    assert window.GetRenderer() is None
    renderer = window.CreateRenderer()
    assert window.GetRenderer() is renderer  # no borrowed Renderer objects
    del renderer
    gc.collect()
    assert window.GetRenderer() is None  # dropping the owner destroyed it


def test_destroying_window_invalidates_its_renderer(sdl):
    # SDL_DestroyWindow also destroys the window's renderer and its textures.
    window = sdl.Window('renderer-window', (16, 16))
    renderer = window.CreateRenderer()
    texture = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA32, sdl.TEXTUREACCESS_STREAMING, (4, 4))
    locked = texture.LockToSurface()
    del window
    gc.collect()
    with pytest.raises(sdl.error):
        renderer.Clear()
    with pytest.raises(sdl.error):
        renderer.RenderFlush()        # SDL doesn't check this one: guarded here
    with pytest.raises(sdl.error):
        renderer.SetRenderTarget(None)
    with pytest.raises(sdl.error):
        texture.Query()
    assert (locked.w, locked.h) == (0, 0)
    del renderer, texture, locked
    gc.collect()


def test_software_window_renderer_frees_window_surface(sdl):
    # A software renderer on a window draws into the window surface and frees
    # it when destroyed: a Surface from GetWindowSurface is emptied, not left
    # dangling, and the surface can't be destroyed from under the renderer.
    window = sdl.Window('software-window', (16, 16))
    renderer = window.CreateRenderer('software')
    surface = window.GetWindowSurface()
    with pytest.raises(sdl.error):
        window.DestroyWindowSurface()
    del renderer
    gc.collect()
    assert (surface.w, surface.h) == (0, 0)
    surface.FillRect(None, (1, 2, 3))


def test_uninitialised_renderer_raises(sdl):
    # SDL_FlushRenderer (and SDL_SetRenderTarget in SDL 3.2) dereference the
    # renderer unchecked: these used to crash.
    renderer = sdl.Renderer()
    with pytest.raises(sdl.error):
        renderer.RenderFlush()
    with pytest.raises(sdl.error):
        renderer.SetRenderTarget(None)
    with pytest.raises(sdl.error):
        renderer.Clear()


def test_module_teardown_order_is_safe(sdl):
    # At interpreter exit module globals are cleared in no particular order;
    # a renderer freed before its textures must not leave them dangling.
    import os
    import subprocess
    import sys
    code = (
        'import SDL3\n'
        'SDL3.Init(SDL3.INIT_VIDEO)\n'
        'window = SDL3.Window("w", (8, 8))\n'
        'renderer = window.CreateRenderer()\n'
        'textures = [SDL3.Texture(renderer, SDL3.PIXELFORMAT_RGBA32, SDL3.TEXTUREACCESS_STATIC, (2, 2))\n'
        '            for _ in range(100)]\n'
        'software = SDL3.CreateSoftwareRenderer(SDL3.CreateRGBSurface((8, 8)))\n'
        'more = [SDL3.Texture(software, size=(2, 2)) for _ in range(10)]\n'
        'del renderer, window\n'
        'print("ok")\n'
    )
    env = dict(os.environ, PYTHONPATH=os.path.dirname(sdl.__file__))
    out = subprocess.run([sys.executable, '-c', code], env=env, capture_output=True, text=True)
    assert out.returncode == 0 and out.stdout.strip() == 'ok', out.stderr
