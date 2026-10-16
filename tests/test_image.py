'''Phase 11: SDL_image - typed loading, format probes, save-to-bytes, SVG, XPM,
animations; SDL_image 3.4's extra savers and animation encoding when present.'''

import pathlib
import struct

import pytest


def _rgba(surface, x, y):
    '''(r, g, b, a) at (x, y), whatever the surface's own format.'''
    import SDL3
    rgba = surface.Convert(SDL3.PIXELFORMAT_RGBA32)
    off = y * rgba.pitch + x * 4
    return tuple(rgba.pixels[off:off + 4])


def _near(got, want, tolerance=8):
    '''Colour comparison for lossy codecs (WEBP).'''
    return all(abs(a - b) <= tolerance for a, b in zip(got, want))


def _gif(frames, delay_cs=10):
    '''A minimal 1x1 animated GIF; frames is a list of palette indexes (0 = red,
    1 = blue). The LZW stream is clear, <index>, end at code size 2.'''
    lzw = {0: b'\x02\x02\x44\x01\x00', 1: b'\x02\x02\x4c\x01\x00'}
    out = bytearray(b'GIF89a' + struct.pack('<HHBBB', 1, 1, 0x80, 0, 0))
    out += b'\xff\x00\x00' + b'\x00\x00\xff'  # 2-entry global palette
    out += b'\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00'  # loop forever
    for index in frames:
        out += b'\x21\xf9\x04\x04' + struct.pack('<H', delay_cs) + b'\x00\x00'
        out += b'\x2c' + struct.pack('<HHHHB', 0, 0, 1, 1, 0) + lzw[index]
    out += b'\x3b'
    return bytes(out)


def _tga():
    '''A 2x1 uncompressed 24-bit TGA (blue, red); TGA has no magic number.'''
    header = struct.pack('<BBBHHBHHHHBB', 0, 0, 2, 0, 0, 0, 0, 0, 2, 1, 24, 0x20)
    return header + b'\xff\x00\x00' + b'\x00\x00\xff'


SVG = (b'<svg xmlns="http://www.w3.org/2000/svg" width="10" height="20">'
       b'<rect width="10" height="20" fill="#00ff00"/></svg>')

XPM = [
    '2 2 2 1',
    'r c #FF0000',
    'b c #0000FF',
    'rb',
    'br',
]

PROBES = ('AVIF', 'BMP', 'CUR', 'GIF', 'ICO', 'JPG', 'JXL', 'LBM', 'PCX', 'PNG',
          'PNM', 'QOI', 'SVG', 'TIF', 'WEBP', 'XCF', 'XPM', 'XV')


class _BytesPath:
    '''An os.PathLike whose path is bytes: rejected (bytes means contents).'''
    def __fspath__(self):
        return b'/tmp/x.png'


@pytest.fixture
def red_green(sdl):
    '''4x2 surface: left half red, right half green.'''
    surface = sdl.CreateSurface((4, 2), sdl.PIXELFORMAT_RGBA32)
    surface.FillRect((0, 0, 2, 2), (255, 0, 0, 255))
    surface.FillRect((2, 0, 2, 2), (0, 255, 0, 255))
    return surface


def _needs_34(sdl, name):
    if not hasattr(sdl, name):
        pytest.skip('needs SDL_image >= 3.4')


# --- version ---------------------------------------------------------------

def test_version(sdl):
    major, minor, micro = sdl.IMG_Version()
    assert major == 3 and minor >= 2 and micro >= 0
    assert sdl.IMG_Linked_Version() == sdl.IMG_Version()  # pysdl2 name


def test_no_img_init(sdl):
    # SDL3_image detects codecs itself; IMG_Init / IMG_Quit are gone.
    assert not hasattr(sdl, 'IMG_Init') and not hasattr(sdl, 'IMG_Quit')


# --- saving ------------------------------------------------------------------

def test_save_png_to_bytes_round_trips(sdl, red_green):
    data = red_green.SavePNG()
    assert data[:4] == b'\x89PNG'
    loaded = sdl.LoadImage(data)
    assert (loaded.w, loaded.h) == (4, 2)
    assert _rgba(loaded, 0, 0) == (255, 0, 0, 255)
    assert _rgba(loaded, 3, 1) == (0, 255, 0, 255)


def test_save_jpg_to_bytes(sdl, red_green):
    small = red_green.SaveJPG(quality=10)
    big = red_green.SaveJPG(quality=100)
    assert small[:2] == big[:2] == b'\xff\xd8'
    assert sdl.IMG_isJPG(big) is True
    loaded = sdl.LoadImage(big)
    assert (loaded.w, loaded.h) == (4, 2)


def test_save_bmp_to_bytes(sdl, red_green, tmp_path):
    data = red_green.SaveBMP()
    assert data[:2] == b'BM'
    assert _rgba(sdl.LoadBMP(data), 3, 0) == (0, 255, 0, 255)
    path = tmp_path / 's.bmp'
    assert red_green.SaveBMP(str(path)) is None
    assert path.read_bytes() == data


def test_save_to_path_still_works(sdl, red_green, tmp_path):
    path = tmp_path / 'x.png'
    assert red_green.SavePNG(str(path)) is None
    assert path.read_bytes() == red_green.SavePNG()
    assert red_green.SavePNG(tmp_path / 'p.png') is None  # os.PathLike
    assert (tmp_path / 'p.png').read_bytes()[:4] == b'\x89PNG'
    assert red_green.SaveJPG(str(tmp_path / 'x.jpg'), quality=50) is None
    with pytest.raises(sdl.error):
        red_green.SavePNG(str(tmp_path / 'missing-dir' / 'x.png'))
    with pytest.raises(TypeError):
        red_green.SavePNG(b'x.png')  # bytes is contents, never a path
    with pytest.raises(TypeError):
        red_green.SavePNG(_BytesPath())


def test_save_rejects_unusable_surfaces(sdl):
    # SDL_image 3.2's PNG writer reports success having written nothing for an
    # indexed surface with no palette (3.4 refuses); both now raise.
    indexed = sdl.CreateSurface((4, 4), sdl.PIXELFORMAT_INDEX8)
    for save in (indexed.SavePNG, indexed.SaveBMP, indexed.SaveJPG):
        with pytest.raises(sdl.error, match='palette'):
            save()
    palette = sdl.Palette(256)
    indexed.SetPalette(palette)
    assert indexed.SavePNG()[:4] == b'\x89PNG'

    empty = type(indexed)()  # an uninitialised wrapper
    with pytest.raises(sdl.error):
        empty.SavePNG()


def test_save_avif(sdl, red_green, tmp_path):
    try:
        data = red_green.SaveAVIF(quality=80)
    except sdl.error as exc:
        pytest.skip(f'no AVIF encoder: {exc}')
    assert data[4:12] == b'ftypavif'
    assert sdl.IMG_isAVIF(data) is True
    assert red_green.SaveAVIF(tmp_path / 'a.avif') is None


def test_save_34_formats_round_trip(sdl, red_green, tmp_path):
    _needs_34(sdl, 'IMG_isANI')
    for name, magic, probe in (('SaveGIF', b'GIF8', 'GIF'), ('SaveTGA', None, None),
                               ('SaveICO', b'\x00\x00\x01\x00', 'ICO'),
                               ('SaveCUR', b'\x00\x00\x02\x00', 'CUR'),
                               ('SaveWEBP', b'RIFF', 'WEBP')):
        save = getattr(red_green, name)
        data = save()
        if magic:
            assert data.startswith(magic), name
        if probe:
            assert getattr(sdl, 'IMG_is' + probe)(data) is True, name
        loaded = sdl.LoadImage(data, type='TGA' if name == 'SaveTGA' else None)
        assert (loaded.w, loaded.h) == (4, 2), name
        assert _near(_rgba(loaded, 0, 0)[:3], (255, 0, 0)), name  # WEBP is lossy
        assert save(tmp_path / ('o.' + name[4:].lower())) is None
    # WEBP quality is a float, as in SDL; lossy output still round-trips.
    assert sdl.IMG_isWEBP(red_green.SaveWEBP(quality=12.5))


def test_save_typed(sdl, red_green, tmp_path):
    _needs_34(sdl, 'IMG_isANI')
    assert red_green.Save(type='png') == red_green.SavePNG()
    assert red_green.Save(type='BMP')[:2] == b'BM'
    path = tmp_path / 'by-extension.gif'
    assert red_green.Save(path) is None  # type from the extension
    assert sdl.IMG_isGIF(str(path)) is True
    assert red_green.Save(str(tmp_path / 'x.dat'), type='jpg') is None
    assert sdl.IMG_isJPG(str(tmp_path / 'x.dat')) is True
    with pytest.raises(TypeError):
        red_green.Save()  # neither a path nor a type
    with pytest.raises(ValueError):
        red_green.Save(str(tmp_path / 'noext'))
    with pytest.raises(sdl.error):
        red_green.Save(type='nonsense')


# --- loading -----------------------------------------------------------------

def test_load_image_sources(sdl, red_green, tmp_path):
    path = tmp_path / 'img.png'
    red_green.SavePNG(str(path))
    for src in (str(path), path, path.read_bytes(), bytearray(path.read_bytes()),
                memoryview(path.read_bytes())):
        assert sdl.LoadImage(src).w == 4
    with pytest.raises(sdl.error):
        sdl.LoadImage(str(tmp_path / 'nope.png'))
    with pytest.raises(sdl.error):
        sdl.LoadImage(b'')
    with pytest.raises(TypeError):
        sdl.LoadImage(42)
    with pytest.raises(TypeError):
        sdl.LoadImage(_BytesPath())


def test_load_image_typed(sdl, red_green, tmp_path):
    data = red_green.SavePNG()
    assert sdl.LoadImage(data, type='PNG').w == 4
    # Formats with a magic number are detected from the data whatever `type` says.
    assert sdl.LoadImage(data, 'GIF').h == 2

    tga = _tga()
    with pytest.raises(sdl.error):
        sdl.LoadImage(tga)  # magic-less: undetectable without a type
    surface = sdl.LoadImage(tga, type='TGA')
    assert (surface.w, surface.h) == (2, 1)
    assert _rgba(surface, 0, 0)[:3] == (0, 0, 255)
    assert _rgba(surface, 1, 0)[:3] == (255, 0, 0)

    # A path with no type still names the format by its extension.
    path = tmp_path / 'p.tga'
    path.write_bytes(tga)
    assert sdl.LoadImage(path).w == 2
    assert sdl.LoadImage(str(path), type='TGA').w == 2


def test_load_bmp_accepts_pathlike(sdl, red_green, tmp_path):
    path = tmp_path / 'p.bmp'
    red_green.SaveBMP(str(path))
    assert sdl.LoadBMP(path).w == 4
    assert sdl.LoadBMP(pathlib.Path(str(path))).h == 2
    assert sdl.LoadBMP(memoryview(path.read_bytes())).w == 4
    with pytest.raises(sdl.error):
        sdl.LoadBMP(b'')


def test_format_probes(sdl, red_green, tmp_path):
    png = red_green.SavePNG()
    bmp_path = tmp_path / 'p.bmp'
    red_green.SaveBMP(str(bmp_path))
    bmp = bmp_path.read_bytes()

    assert sdl.IMG_isPNG(png) is True
    assert sdl.IMG_isBMP(png) is False
    assert sdl.IMG_isBMP(bmp) is True
    assert sdl.IMG_isBMP(str(bmp_path)) is True  # probes accept paths too
    assert sdl.IMG_isBMP(bmp_path) is True
    assert sdl.IMG_isGIF(_gif([0])) is True
    assert sdl.IMG_isSVG(SVG) is True
    assert sdl.IMG_isXPM(b'/* XPM */\nstatic char *x[] = {};') is True
    names = PROBES + (('ANI',) if hasattr(sdl, 'IMG_isANI') else ())
    for name in names:
        probe = getattr(sdl, 'IMG_is' + name)
        assert probe(b'not an image at all') is False, name
        assert probe(b'') is False, name
    with pytest.raises(sdl.error):
        sdl.IMG_isPNG(str(tmp_path / 'missing.png'))


def test_probe_does_not_consume_bytes(sdl, red_green):
    data = red_green.SavePNG()
    assert sdl.IMG_isPNG(data) and sdl.IMG_isPNG(data)
    assert sdl.LoadImage(data).w == 4


def test_load_sized_svg(sdl, tmp_path):
    natural = sdl.LoadImage(SVG)
    assert (natural.w, natural.h) == (10, 20)
    sized = sdl.IMG_LoadSizedSVG(SVG, 30, 60)
    assert (sized.w, sized.h) == (30, 60)
    assert _rgba(sized, 15, 30)[:3] == (0, 255, 0)
    path = tmp_path / 'i.svg'
    path.write_bytes(SVG)
    assert sdl.IMG_LoadSizedSVG(path, 0, 40).w == 20  # 0 keeps the aspect ratio
    with pytest.raises(sdl.error):
        sdl.IMG_LoadSizedSVG(b'not svg', 8, 8)


# --- XPM ---------------------------------------------------------------------

def test_read_xpm_from_array(sdl):
    surface = sdl.IMG_ReadXPMFromArray(XPM)
    assert (surface.w, surface.h) == (2, 2)
    assert _rgba(surface, 0, 0)[:3] == (255, 0, 0)
    assert _rgba(surface, 1, 0)[:3] == (0, 0, 255)
    rgb = sdl.IMG_ReadXPMFromArrayToRGB888(XPM)
    assert sdl.PixelFormat(rgb.format).bytes == 4  # never paletted
    assert _rgba(rgb, 0, 1)[:3] == (0, 0, 255)
    assert sdl.IMG_ReadXPMFromArray(tuple(XPM)).w == 2
    two = ['2 1 2 2', 'aa c #FF0000', 'bb c None', 'aabb']  # 2 chars per pixel
    assert _rgba(sdl.IMG_ReadXPMFromArray(two), 0, 0) == (255, 0, 0, 255)
    with pytest.raises(TypeError):
        sdl.IMG_ReadXPMFromArray([1, 2])
    with pytest.raises(TypeError):
        sdl.IMG_ReadXPMFromArray(42)


@pytest.mark.parametrize('lines', [
    [],
    ['garbage'],
    ['+1 1 1 1', 'a c #FF0000', 'a'],          # signs are not accepted
    ['0 1 1 1', 'a c #FF0000', ''],
    ['1234567890 1 1 1', 'a c #FF0000', 'a'],  # > 9 digits
    ['1 2 1 1', 'a c #FF0000', 'a'],           # a row missing
    ['1 1 2 1', 'a c #FF0000', 'a'],           # a colour missing
    ['1 1 1 2', 'ab', 'ab'],                   # colour line < cpp + 1
    ['3 1 1 1', 'a c #FF0000', 'aa'],          # row < width * cpp
    ['2 1 1 2', 'aa c #FF0000', 'aaa'],
    ['1 1 1 1', 'a c #FF0000', 'b'],           # undefined colour (NULL deref before 3.4.4)
    ['2 2 1 1', 'a c #FF0000', 'aa', 'ab'],
])
def test_read_xpm_rejects_malformed(sdl, lines):
    # SDL_image reads the array without bounds checks, so malformed input is
    # caught before it gets there.
    for read in (sdl.IMG_ReadXPMFromArray, sdl.IMG_ReadXPMFromArrayToRGB888):
        with pytest.raises(ValueError):
            read(lines)


def test_read_xpm_sdl_errors(sdl):
    # Well-formed lines, but a colour SDL_image cannot parse.
    with pytest.raises(sdl.error):
        sdl.IMG_ReadXPMFromArray(['1 1 1 1', 'a c notacolour', 'a'])


# --- animation ---------------------------------------------------------------

def test_load_animation(sdl, tmp_path):
    gif = _gif([0, 1, 0], delay_cs=7)
    w, h, frames = sdl.IMG_LoadAnimation(gif)
    assert (w, h) == (1, 1)
    assert [delay for _, delay in frames] == [70, 70, 70]
    colours = [_rgba(frame, 0, 0)[:3] for frame, _ in frames]
    assert colours == [(255, 0, 0), (0, 0, 255), (255, 0, 0)]

    path = tmp_path / 'anim.gif'
    path.write_bytes(gif)
    assert len(sdl.IMG_LoadAnimation(str(path))[2]) == 3
    assert len(sdl.IMG_LoadAnimation(path)[2]) == 3
    assert len(sdl.IMG_LoadAnimation(path.read_bytes(), type='GIF')[2]) == 3


def test_load_animation_still_image(sdl, red_green):
    # A still image comes back as a one-frame animation.
    w, h, frames = sdl.IMG_LoadAnimation(red_green.SaveBMP())
    assert (w, h, len(frames)) == (4, 2, 1)
    assert _rgba(frames[0][0], 3, 0) == (0, 255, 0, 255)


def test_animation_frames_outlive_each_other(sdl):
    _, _, frames = sdl.IMG_LoadAnimation(_gif([0, 1]))
    second = frames[1][0]
    del frames  # frees the first frame; the second is independently owned
    assert _rgba(second, 0, 0)[:3] == (0, 0, 255)


def test_load_animation_rejects_garbage(sdl):
    with pytest.raises(sdl.error):
        sdl.IMG_LoadAnimation(b'nope')
    with pytest.raises(TypeError):
        sdl.IMG_LoadAnimation(None)


def test_save_animation_round_trips(sdl, tmp_path):
    _needs_34(sdl, 'IMG_SaveAnimation')
    _, _, frames = sdl.IMG_LoadAnimation(_gif([0, 1, 0], delay_cs=7))
    for kind in ('GIF', 'WEBP', 'APNG'):
        data = sdl.IMG_SaveAnimation(frames, type=kind)
        w, h, back = sdl.IMG_LoadAnimation(data)
        assert (w, h) == (1, 1), kind
        assert [delay for _, delay in back] == [70, 70, 70], kind
        colours = [_rgba(f, 0, 0)[:3] for f, _ in back]
        assert all(map(_near, colours, [(255, 0, 0), (0, 0, 255), (255, 0, 0)])), kind
    ani = sdl.IMG_SaveAnimation(frames, type='ANI')
    assert sdl.IMG_isANI(ani) is True
    assert len(sdl.IMG_LoadAnimation(ani)[2]) == 3

    path = tmp_path / 'a.gif'
    assert sdl.IMG_SaveAnimation(frames, path) is None  # type from the extension
    assert len(sdl.IMG_LoadAnimation(path)[2]) == 3
    assert sdl.IMG_SaveAnimation(frames, str(tmp_path / 'a.bin'), type='webp') is None
    assert sdl.IMG_isWEBP(str(tmp_path / 'a.bin')) is True


def test_save_animation_rejects_bad_frames(sdl, red_green):
    _needs_34(sdl, 'IMG_SaveAnimation')
    one = sdl.CreateSurface((1, 1), sdl.PIXELFORMAT_RGBA32)
    with pytest.raises(TypeError):
        sdl.IMG_SaveAnimation([(one, 10)])  # no path, no type
    with pytest.raises(ValueError):
        sdl.IMG_SaveAnimation([], type='GIF')
    with pytest.raises(ValueError):
        sdl.IMG_SaveAnimation([(one, 10), (red_green, 10)], type='GIF')  # sizes differ
    with pytest.raises(ValueError):
        sdl.IMG_SaveAnimation([(one, -1)], type='GIF')
    with pytest.raises(TypeError):
        sdl.IMG_SaveAnimation([[one, 10]], type='GIF')
    with pytest.raises(TypeError):
        sdl.IMG_SaveAnimation([(b'x', 10)], type='GIF')
    with pytest.raises(sdl.error):
        sdl.IMG_SaveAnimation([(one, 10)], type='nonsense')
    with pytest.raises(sdl.error):
        sdl.IMG_SaveAnimation([(sdl.CreateSurface((1, 1), sdl.PIXELFORMAT_INDEX8), 10)], type='GIF')


def test_clipboard_image(sdl):
    _needs_34(sdl, 'IMG_GetClipboardImage')
    image = sdl.IMG_GetClipboardImage()  # nothing on the dummy driver's clipboard
    assert image is None or image.w > 0


# --- textures ----------------------------------------------------------------

def test_renderer_load_texture_sources(sdl, renderer, red_green, tmp_path):
    data = red_green.SavePNG()
    path = tmp_path / 't.png'
    path.write_bytes(data)
    for texture in (renderer.LoadTexture(str(path)), renderer.LoadTexture(data),
                    renderer.LoadTexture(data, type='PNG'), renderer.LoadTexture(path),
                    renderer.LoadTexture(src=bytearray(data))):
        assert texture.Query()[2:] == (4, 2)
    assert renderer.LoadTexture(_tga(), type='TGA').Query()[2:] == (2, 1)
    with pytest.raises(sdl.error):
        renderer.LoadTexture(b'garbage')
    with pytest.raises(TypeError):
        renderer.LoadTexture(3.5)
