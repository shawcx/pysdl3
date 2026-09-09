'''Phase 1: Renderer construction, primitives, draw state, targets, info.'''

import pytest


def test_standalone_constructor(sdl, window):
    renderer = sdl.Renderer(window)
    assert type(renderer).__name__ == 'Renderer'
    assert sdl.Renderer in type(renderer).__mro__


def test_constructor_rejects_non_window(sdl):
    with pytest.raises(TypeError):
        sdl.Renderer(object())


def test_software_renderer_from_surface(sdl):
    surface = sdl.CreateRGBSurface((32, 32))
    renderer = sdl.CreateSoftwareRenderer(surface)
    renderer.SetRenderDrawColor(255, 255, 255, 255)
    renderer.Clear()
    with pytest.raises(TypeError):
        sdl.CreateSoftwareRenderer(object())


def test_draw_color_roundtrip(renderer):
    renderer.SetRenderDrawColor(10, 20, 30, 40)
    assert renderer.GetRenderDrawColor() == (10, 20, 30, 40)


def test_primitives_accept_ints_and_floats(renderer):
    renderer.Clear()
    renderer.DrawPoint(1, 2)
    renderer.DrawPoint(1.5, 2.5)
    renderer.DrawPoints([(0, 0), (1, 1), (2, 2)])
    renderer.DrawLine(0, 0, 10, 10)
    renderer.DrawLines([(0, 0), (5, 5), (10, 0)])
    renderer.DrawRect((1, 1, 5, 5))
    renderer.DrawRect(None)
    renderer.DrawRects([(0, 0, 4, 4), (5, 5, 4, 4)])
    renderer.FillRect((2, 2, 8, 8))
    renderer.FillRect(None)
    renderer.FillRects([(0, 0, 2, 2), (3, 3, 2, 2)])
    renderer.Present()


def test_draw_blend_mode_roundtrip(renderer, sdl):
    renderer.SetRenderDrawBlendMode(sdl.BLENDMODE_BLEND)
    assert renderer.GetRenderDrawBlendMode() == sdl.BLENDMODE_BLEND


def test_viewport_clip_scale_logical(renderer, sdl):
    renderer.RenderSetViewport((0, 0, 32, 24))
    assert renderer.RenderGetViewport() == (0, 0, 32, 24)
    renderer.RenderSetViewport(None)

    assert renderer.RenderIsClipEnabled() is False
    renderer.RenderSetClipRect((1, 1, 10, 10))
    assert renderer.RenderGetClipRect() == (1, 1, 10, 10)
    assert renderer.RenderIsClipEnabled() is True
    renderer.RenderSetClipRect(None)
    assert renderer.RenderIsClipEnabled() is False

    renderer.RenderSetScale(2.0, 3.0)
    assert renderer.RenderGetScale() == (2.0, 3.0)
    renderer.RenderSetScale(1.0, 1.0)

    renderer.RenderSetLogicalSize(100, 80)
    assert renderer.RenderGetLogicalSize()[:2] == (100, 80)
    renderer.RenderSetLogicalSize(0, 0, sdl.LOGICAL_PRESENTATION_DISABLED)


def test_render_target_identity(renderer, sdl):
    assert renderer.GetRenderTarget() is None

    target = sdl.Texture(renderer, sdl.PIXELFORMAT_RGBA8888, sdl.TEXTUREACCESS_TARGET, (16, 16))
    renderer.SetRenderTarget(target)
    assert renderer.GetRenderTarget() is target
    renderer.SetRenderTarget(None)
    assert renderer.GetRenderTarget() is None


def test_read_pixels_length_and_value(renderer):
    renderer.SetRenderDrawColor(50, 60, 70, 255)
    renderer.Clear()
    data = renderer.RenderReadPixels((0, 0, 2, 2))
    assert len(data) == 2 * 2 * 4
    # ARGB8888, little-endian in memory -> B, G, R, A
    assert data[0:4] == bytes((70, 60, 50, 255))


def test_info_and_output_size(renderer):
    name, formats, max_w, max_h = renderer.GetRendererInfo()
    assert isinstance(name, str) and name
    assert isinstance(formats, list)
    assert renderer.GetRendererOutputSize() == (64, 48)


def test_render_geometry(renderer, sdl):
    verts = [
        ((0, 0), (255, 0, 0, 255)),
        ((16, 0), (0, 255, 0, 255)),
        ((0, 16), (0, 0, 255, 255)),
    ]
    renderer.RenderGeometry(None, verts)
    renderer.RenderGeometry(None, verts, [0, 1, 2])
    with pytest.raises(ValueError):
        renderer.RenderGeometry(None, [((0, 0),)])


def test_compose_custom_blend_mode(sdl):
    mode = sdl.ComposeCustomBlendMode(
        sdl.BLENDFACTOR_SRC_ALPHA, sdl.BLENDFACTOR_ONE, sdl.BLENDOPERATION_ADD,
        sdl.BLENDFACTOR_ONE, sdl.BLENDFACTOR_ONE, sdl.BLENDOPERATION_ADD)
    assert isinstance(mode, int)
    assert mode != sdl.BLENDMODE_INVALID
