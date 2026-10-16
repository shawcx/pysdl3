'''Phase 2: Surface creation, blitting, fills, conversion, state.'''

import pytest


@pytest.fixture
def surface(sdl):
    return sdl.CreateRGBSurfaceWithFormat((16, 16), sdl.PIXELFORMAT_RGBA8888)


def _pixel(surface, x, y):
    data = surface.pixels
    off = y * surface.pitch + x * 4
    return tuple(data[off:off + 4])


def test_create_with_format(sdl, surface):
    assert (surface.w, surface.h) == (16, 16)
    assert surface.format == sdl.PIXELFORMAT_RGBA8888
    assert surface.pitch == 16 * 4


def test_create_from_buffer_keeps_data_alive(sdl):
    buf = bytearray(b'\xAB' * (4 * 4 * 4))
    surface = sdl.CreateRGBSurfaceFrom(buf, (4, 4))
    del buf
    assert surface.pixels[:4] == b'\xAB\xAB\xAB\xAB'


def test_fill_rect_int_and_tuple_colours(sdl, surface):
    surface.FillRect(None, (0, 0, 0, 255))
    surface.FillRect((0, 0, 8, 8), (255, 0, 0, 255))
    surface.FillRect((8, 8, 8, 8), surface.MapRGB((0, 255, 0)))
    # RGBA8888 little-endian in memory -> A, B, G, R
    assert _pixel(surface, 0, 0) == (255, 0, 0, 255)
    assert _pixel(surface, 15, 15) == (255, 0, 255, 0)
    assert _pixel(surface, 0, 15) == (255, 0, 0, 0)


def test_fill_rects(sdl, surface):
    surface.FillRect(None, 0)
    surface.FillRects([(0, 0, 2, 2), (4, 4, 2, 2)], (1, 2, 3, 4))


def test_blit_and_blit_scaled(sdl, surface):
    src = sdl.CreateRGBSurfaceWithFormat((4, 4), sdl.PIXELFORMAT_RGBA8888)
    src.FillRect(None, (10, 20, 30, 255))
    surface.FillRect(None, 0)
    surface.Blit(src, None, (2, 2))
    assert _pixel(surface, 3, 3) == (255, 30, 20, 10)
    surface.BlitScaled(src, None, (0, 0, 16, 16))
    assert _pixel(surface, 0, 0) == (255, 30, 20, 10)


def test_blit_rejects_non_surface(surface):
    with pytest.raises(TypeError):
        surface.Blit(object())


def test_convert(sdl, surface):
    surface.FillRect(None, (1, 2, 3, 255))

    by_enum = surface.Convert(sdl.PIXELFORMAT_RGB24)
    assert by_enum.format == sdl.PIXELFORMAT_RGB24

    by_format = surface.Convert(sdl.PixelFormat(sdl.PIXELFORMAT_BGRA8888))
    assert by_format.format == sdl.PIXELFORMAT_BGRA8888

    with pytest.raises(TypeError):
        surface.Convert('nope')


def test_duplicate(surface):
    surface.FillRect(None, (9, 9, 9, 255))
    clone = surface.Duplicate()
    assert (clone.w, clone.h) == (surface.w, surface.h)
    assert clone.pixels == surface.pixels


def test_colour_key_roundtrip(sdl, surface):
    assert surface.GetColorKey() is None
    surface.SetColorKey((255, 0, 255))
    assert surface.GetColorKey() == surface.MapRGBA((255, 0, 255, 255))
    surface.SetColorKey((255, 0, 255), enable=False)
    assert surface.GetColorKey() is None


def test_blend_colour_alpha_mod(sdl, surface):
    surface.SetBlendMode(sdl.BLENDMODE_BLEND)
    assert surface.GetBlendMode() == sdl.BLENDMODE_BLEND
    surface.SetColorMod(200, 100, 50)
    assert surface.GetColorMod() == (200, 100, 50)
    surface.SetAlphaMod(123)
    assert surface.GetAlphaMod() == 123


def test_clip_rect(surface):
    assert surface.SetClipRect((1, 1, 8, 8)) is True
    assert surface.GetClipRect() == (1, 1, 8, 8)
    surface.SetClipRect(None)
    assert surface.GetClipRect() == (0, 0, 16, 16)


def test_rle_and_get_pixel_format(sdl, surface):
    surface.SetRLE(True)
    surface.SetRLE(False)
    pf = surface.GetPixelFormat()
    assert pf.format == sdl.PIXELFORMAT_RGBA8888


def test_load_bmp_from_bytes(sdl, tmp_path):
    src = sdl.CreateRGBSurface((8, 6))
    src.FillRect(None, (1, 2, 3, 255))
    path = tmp_path / 's.bmp'
    src.SaveBMP(str(path))

    loaded = sdl.LoadBMP(path.read_bytes())
    assert (loaded.w, loaded.h) == (8, 6)


def test_save_png_jpg(sdl, surface, tmp_path):
    surface.FillRect(None, (40, 50, 60, 255))
    png = tmp_path / 'o.png'
    jpg = tmp_path / 'o.jpg'
    surface.SavePNG(str(png))
    surface.SaveJPG(str(jpg), quality=75)
    assert png.read_bytes()[:4] == b'\x89PNG'
    assert jpg.read_bytes()[:2] == b'\xff\xd8'


# --- phase 12: pixel conversion, surface queries, stretching ----------------

def test_convert_pixels(sdl):
    src = bytes([10, 20, 30, 128]) * 4  # 2x2 RGBA32
    bgra = sdl.ConvertPixels((2, 2), sdl.PIXELFORMAT_RGBA32, src, sdl.PIXELFORMAT_BGRA32)
    assert bgra == bytes([30, 20, 10, 128]) * 4
    assert len(sdl.ConvertPixels((2, 2), sdl.PIXELFORMAT_RGBA32, src, sdl.PIXELFORMAT_RGB24)) == 12
    padded = sdl.ConvertPixels((2, 2), sdl.PIXELFORMAT_RGBA32, src, sdl.PIXELFORMAT_RGBA32, dst_pitch=12)
    assert len(padded) == 24 and padded[:8] == src[:8] and padded[12:20] == src[8:]
    assert sdl.ConvertPixels((1, 2), sdl.PIXELFORMAT_RGBA32, src, sdl.PIXELFORMAT_RGBA32, src_pitch=8) == src[:4] + src[8:12]
    # YUV: luma rows, then the chroma planes
    assert len(sdl.ConvertPixels((2, 2), sdl.PIXELFORMAT_RGBA32, src, sdl.PIXELFORMAT_IYUV)) == 6
    assert len(sdl.ConvertPixels((2, 2), sdl.PIXELFORMAT_RGBA32, src, sdl.PIXELFORMAT_NV12)) == 6
    assert len(sdl.ConvertPixels((3, 2), sdl.PIXELFORMAT_RGBA32, bytes(24), sdl.PIXELFORMAT_YUY2)) == 16
    assert len(sdl.ConvertPixels((3, 3), sdl.PIXELFORMAT_NV12, bytes(17), sdl.PIXELFORMAT_IYUV)) == 17
    back = sdl.ConvertPixels((2, 2), sdl.PIXELFORMAT_IYUV,
                             sdl.ConvertPixels((2, 2), sdl.PIXELFORMAT_RGBA32, src, sdl.PIXELFORMAT_IYUV),
                             sdl.PIXELFORMAT_RGBA32)
    assert len(back) == 16 and back[3] == 255


def test_convert_pixels_checks_sizes(sdl):
    src = bytes(16)
    bad = [
        ((2, 2), sdl.PIXELFORMAT_RGBA32, src[:-1], sdl.PIXELFORMAT_BGRA32, {}),       # short src
        ((0, 2), sdl.PIXELFORMAT_RGBA32, src, sdl.PIXELFORMAT_BGRA32, {}),
        ((2, 2), sdl.PIXELFORMAT_RGBA32, src, sdl.PIXELFORMAT_BGRA32, {'src_pitch': 4}),   # < a row
        ((2, 2), sdl.PIXELFORMAT_RGBA32, src, sdl.PIXELFORMAT_BGRA32, {'dst_pitch': 7}),
        ((2, 2), sdl.PIXELFORMAT_RGBA32, src, sdl.PIXELFORMAT_BGRA32, {'src_pitch': -1}),
        ((3, 3), sdl.PIXELFORMAT_NV12, bytes(16), sdl.PIXELFORMAT_RGBA32, {}),         # needs 9 + 8
        ((2, 2), sdl.PIXELFORMAT_P010, bytes(12), sdl.PIXELFORMAT_RGBA32, {'src_pitch': 5}),  # odd
        ((8, 1), sdl.PIXELFORMAT_INDEX1LSB, bytes(1), sdl.PIXELFORMAT_RGBA32, {}),     # no byte layout
    ]
    for size, sf, data, df, kwargs in bad:
        with pytest.raises(ValueError):
            sdl.ConvertPixels(size, sf, data, df, **kwargs)


def test_premultiply_alpha(sdl):
    # ARGB8888 little-endian bytes are B, G, R, A.
    pixel = bytes([200, 100, 50, 128])
    out = sdl.PremultiplyAlpha((1, 1), sdl.PIXELFORMAT_ARGB8888, pixel, sdl.PIXELFORMAT_ARGB8888)
    assert out == bytes([100, 50, 25, 128])
    linear = sdl.PremultiplyAlpha((1, 1), sdl.PIXELFORMAT_ARGB8888, pixel, sdl.PIXELFORMAT_ARGB8888, linear=True)
    assert linear[3] == 128 and linear != out
    with pytest.raises(ValueError):
        sdl.PremultiplyAlpha((2, 1), sdl.PIXELFORMAT_ARGB8888, pixel, sdl.PIXELFORMAT_ARGB8888)


def test_has_color_key_and_rle(sdl, surface):
    assert surface.HasColorKey() is False
    surface.SetColorKey((0, 0, 0))
    assert surface.HasColorKey() is True
    assert surface.HasRLE() is False
    surface.SetRLE(True)
    assert surface.HasRLE() is True


def test_stretch(sdl):
    small = sdl.CreateSurface((2, 2), sdl.PIXELFORMAT_RGBA32)
    small.FillRect(None, (0, 200, 0, 255))
    big = sdl.CreateSurface((8, 8), sdl.PIXELFORMAT_RGBA32)
    big.SoftStretchLinear(small)
    off = 4 * big.pitch + 4 * 4
    assert tuple(big.pixels[off:off + 4]) == (0, 200, 0, 255)
    big.FillRect(None, (0, 0, 0, 255))
    big.Stretch(small, None, (0, 0, 4, 4), sdl.SCALEMODE_NEAREST)
    assert tuple(big.pixels[:4]) == (0, 200, 0, 255)
    assert tuple(big.pixels[off:off + 4]) == (0, 0, 0, 255)
    big.Stretch(small, dstrect=(4, 4, 4, 4), scale_mode=sdl.SCALEMODE_LINEAR)
    assert tuple(big.pixels[off:off + 4]) == (0, 200, 0, 255)
    with pytest.raises(sdl.error):  # bilinear needs 32-bit pixels
        sdl.CreateSurface((8, 8), sdl.PIXELFORMAT_RGB24).SoftStretchLinear(sdl.CreateSurface((2, 2), sdl.PIXELFORMAT_RGB24))
    with pytest.raises(sdl.error):
        big.SoftStretch(small, (0, 0, 4, 4))  # source rect outside the source


def test_stretch_checks_rects(sdl):
    # SDL_StretchSurface trusts rect sizes: a 2-item rect (w = h = -1) or a
    # zero-width source read outside the rows (the first crashed), and a YUV
    # destination is written unclipped.
    src = sdl.CreateSurface((16, 16), sdl.PIXELFORMAT_RGBA32)
    dst = sdl.CreateSurface((64, 64), sdl.PIXELFORMAT_RGBA32)
    with pytest.raises(ValueError):
        dst.SoftStretch(src, (1, 1))
    with pytest.raises(ValueError):
        dst.Stretch(src, None, (0, 0, -4, 4))
    dst.SoftStretch(src, (16, 0, 0, 16))    # nothing to copy
    dst.SoftStretch(src, None, (0, 0, 0, 0))
    yuv = sdl.CreateSurface((4, 4), sdl.PIXELFORMAT_NV12)
    with pytest.raises(ValueError):
        yuv.SoftStretch(src, None, (2, 2, 4, 4))
    yuv.SoftStretch(src, None, (0, 0, 4, 4))
    yuv.SoftStretch(src)


def test_uninitialised_surface_raises(sdl, surface):
    blank = type(surface)()
    for name in ('w', 'h', 'pitch', 'format', 'pixels', 'bpp'):
        with pytest.raises(sdl.error):
            getattr(blank, name)
    with pytest.raises(sdl.error):
        blank.pixels = b'\0'
    with pytest.raises(sdl.error):
        blank.FillRect(None, (1, 2, 3))
    with pytest.raises(sdl.error):
        blank.GetPixelFormat()
    assert blank.HasColorKey() is False
