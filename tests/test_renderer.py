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


# --- phase 12: window link, vsync, coordinate mapping, raw geometry --------

import array  # noqa: E402
import struct  # noqa: E402

TRIANGLE = struct.pack('6f', 0, 0, 20, 0, 0, 20)
RED = struct.pack('4f', 1, 0, 0, 1)  # SDL_FColor


def test_create_window_and_renderer(sdl):
    window, renderer = sdl.CreateWindowAndRenderer('pair', (40, 30))
    assert type(window).__name__ == 'Window' and type(renderer).__name__ == 'Renderer'
    assert renderer.GetRendererOutputSize() == (40, 30)
    assert window.GetRenderer() is renderer
    assert renderer.RenderGetWindow().GetWindowID() == window.GetWindowID()
    with pytest.raises(TypeError):
        sdl.CreateWindowAndRenderer((40, 30))  # SDL3 takes a title first


def test_render_get_window(sdl, window, renderer):
    assert renderer.RenderGetWindow().GetWindowID() == window.GetWindowID()
    assert renderer.GetRenderWindow().GetWindowID() == window.GetWindowID()
    software = sdl.CreateSoftwareRenderer(sdl.CreateRGBSurface((4, 4)))
    assert software.RenderGetWindow() is None


def test_vsync_and_metal(sdl, renderer):
    try:
        renderer.RenderSetVSync(True)
        assert renderer.GetVSync() == 1
        renderer.SetVSync(sdl.RENDERER_VSYNC_DISABLED)
    except sdl.error:
        pass  # a renderer may not support toggling
    assert renderer.GetVSync() in (0, 1, sdl.RENDERER_VSYNC_ADAPTIVE)
    if 'metal' not in renderer.GetRendererInfo()[0]:
        assert renderer.RenderGetMetalLayer() is None
        assert renderer.GetRenderMetalCommandEncoder() is None


def test_window_logical_mapping(sdl, renderer):
    w, h = renderer.GetRendererOutputSize()
    renderer.SetRenderLogicalPresentation(w // 2, h // 2, sdl.LOGICAL_PRESENTATION_LETTERBOX)
    try:
        assert renderer.GetRenderLogicalPresentation() == (w // 2, h // 2, sdl.LOGICAL_PRESENTATION_LETTERBOX)
        assert renderer.GetRenderLogicalPresentationRect() == (0.0, 0.0, w, h)
        assert renderer.RenderWindowToLogical((w // 2, h // 2)) == (w / 4, h / 4)
        assert renderer.RenderCoordinatesFromWindow((w / 2, h / 2)) == (w / 4, h / 4)
        assert renderer.RenderLogicalToWindow((w / 4, h / 4)) == (w / 2, h / 2)
        assert renderer.RenderCoordinatesToWindow((1.5, 0)) == (3.0, 0.0)
    finally:
        renderer.RenderSetLogicalSize(0, 0, sdl.LOGICAL_PRESENTATION_DISABLED)
    with pytest.raises(ValueError):
        renderer.RenderWindowToLogical((1, 2, 3))


def test_render_geometry_raw(sdl, renderer):
    renderer.SetRenderDrawColor(0, 0, 0, 255)
    renderer.Clear()
    renderer.RenderGeometryRaw(None, TRIANGLE, RED, color_stride=0)            # one colour
    assert renderer.RenderReadPixels((2, 2, 1, 1), sdl.PIXELFORMAT_RGBA32) == bytes((255, 0, 0, 255))
    renderer.RenderGeometryRaw(None, TRIANGLE, RED * 3)                        # per vertex
    renderer.RenderGeometryRaw(None, TRIANGLE, RED * 3, indices=bytes([0, 1, 2]), index_size=1)
    renderer.RenderGeometryRaw(None, TRIANGLE, RED * 3,
                               indices=struct.pack('3H', 2, 1, 0), index_size=2)
    renderer.RenderGeometryRaw(None, array.array('f', [0, 0, 20, 0, 0, 20]),
                               array.array('f', [0, 1, 0, 1] * 3), indices=array.array('I', [0, 1, 2]))
    renderer.RenderGeometryRaw(None, bytearray(TRIANGLE), memoryview(RED * 3), num_vertices=3)
    # interleaved (x, y, r, g, b, a) records: num_vertices comes from xy
    packed = array.array('f')
    for x, y in ((0, 0), (20, 0), (0, 20)):
        packed.extend((x, y, 0, 0, 1, 1))
    renderer.RenderGeometryRaw(None, packed, memoryview(packed).cast('B')[8:], xy_stride=24, color_stride=24)
    renderer.Present()


def test_render_geometry_raw_with_texture(sdl, renderer, texture):
    uv = struct.pack('6f', 0, 0, 1, 0, 0, 1)
    renderer.RenderGeometryRaw(texture, TRIANGLE, RED * 3, uv)
    with pytest.raises(sdl.error):
        renderer.RenderGeometryRaw(texture, TRIANGLE, RED * 3)  # a texture needs uv


def test_render_geometry_raw_validation(sdl, renderer):
    bad = [
        ((None, TRIANGLE, RED), {}),                                   # 3 vertices, 1 colour
        ((None, TRIANGLE, RED * 3), {'xy_stride': 6}),                 # not float-aligned
        ((None, TRIANGLE, RED * 3), {'color_stride': -16}),
        ((None, TRIANGLE, RED * 3), {'indices': b'\0', 'index_size': 3}),
        ((None, TRIANGLE, RED * 3), {'indices': b'\0\0\0', 'index_size': 2}),  # odd length
        ((None, TRIANGLE, RED * 3, TRIANGLE[:20]), {}),                # short uv
        ((None, TRIANGLE, RED * 3), {'num_vertices': 4}),              # more than xy holds
        ((None, TRIANGLE, RED * 3), {'num_vertices': -1}),
        ((None, TRIANGLE, RED * 3), {'xy_stride': 0}),                 # count unknown
        ((None, memoryview(b'\0' + TRIANGLE)[1:], RED * 3), {}),       # misaligned floats
    ]
    for args, kwargs in bad:
        with pytest.raises(ValueError):
            renderer.RenderGeometryRaw(*args, **kwargs)
    with pytest.raises(sdl.error):  # SDL range-checks index values
        renderer.RenderGeometryRaw(None, TRIANGLE, RED * 3, indices=bytes([0, 1, 9]), index_size=1)
    with pytest.raises(TypeError):
        renderer.RenderGeometryRaw('nope', TRIANGLE, RED * 3)
    with pytest.raises(TypeError):
        renderer.RenderGeometryRaw(None, [0.0] * 6, RED * 3)  # a buffer, not a list
