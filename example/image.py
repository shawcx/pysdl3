#!/usr/bin/env python3

# SDL_image tour, no window needed: probe a file's format, load it (animated
# GIF / WEBP / APNG frame by frame), and re-encode it to PNG / JPG (and with
# SDL_image 3.4, GIF / WEBP / an animation) in memory.
#
#   python3 example/image.py [image-file]
#
# With no file it builds a small test image, an SVG and an XPM to work on.

import sys
import SDL3

SDL3.Init(SDL3.INIT_VIDEO)
print('SDL_image', '.'.join(map(str, SDL3.IMG_Version())))

FORMATS = [name[6:] for name in dir(SDL3) if name.startswith('IMG_is')]


def probe(data):
    return [fmt for fmt in FORMATS if getattr(SDL3, 'IMG_is' + fmt)(data)] or ['unknown']


def describe(label, data):
    kinds = probe(data)
    print(f'\n{label}: {len(data)} bytes, looks like {", ".join(kinds)}')

    w, h, frames = SDL3.IMG_LoadAnimation(data)
    if len(frames) > 1:
        total = sum(delay for _, delay in frames)
        print(f'  animation {w}x{h}, {len(frames)} frames, {total} ms per loop')
    surface = frames[0][0]
    print(f'  image {surface.w}x{surface.h}, {SDL3.GetPixelFormatName(surface.format)}')

    sizes = [f'PNG {len(surface.SavePNG())}']
    sizes += [f'JPG q{q} {len(surface.SaveJPG(quality=q))}' for q in (30, 90)]
    if hasattr(surface, 'SaveWEBP'):  # SDL_image 3.4
        sizes += [f'GIF {len(surface.SaveGIF())}', f'WEBP {len(surface.SaveWEBP())}']
    print('  re-encoded:', ', '.join(sizes), 'bytes')

    if len(frames) > 1 and hasattr(SDL3, 'IMG_SaveAnimation'):
        apng = SDL3.IMG_SaveAnimation(frames, type='APNG')
        print(f'  as APNG: {len(apng)} bytes, {len(SDL3.IMG_LoadAnimation(apng)[2])} frames')


if len(sys.argv) > 1:
    with open(sys.argv[1], 'rb') as f:
        describe(sys.argv[1], f.read())
else:
    # A gradient drawn into a surface, round-tripped through PNG bytes.
    surface = SDL3.CreateSurface((64, 32), SDL3.PIXELFORMAT_RGBA32)
    for x in range(64):
        surface.FillRect((x, 0, 1, 32), (x * 4, 128, 255 - x * 4, 255))
    describe('generated gradient', surface.SavePNG())

    if hasattr(SDL3, 'IMG_SaveAnimation'):
        # The gradient sliding sideways, as a looping GIF.
        frames = []
        for step in range(4):
            frame = SDL3.CreateSurface((64, 32), SDL3.PIXELFORMAT_RGBA32)
            frame.Blit(surface, None, (step * 16 - 64, 0))
            frame.Blit(surface, None, (step * 16, 0))
            frames.append((frame, 100))
        describe('generated animation', SDL3.IMG_SaveAnimation(frames, type='GIF'))

    svg = (b'<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16">'
           b'<circle cx="8" cy="8" r="7" fill="orange"/></svg>')
    describe('inline SVG', svg)
    big = SDL3.IMG_LoadSizedSVG(svg, 256, 256)
    print(f'  rasterised at {big.w}x{big.h} with IMG_LoadSizedSVG')

    icon = SDL3.IMG_ReadXPMFromArray([
        '4 4 2 1', '. c None', '# c #3060FF',
        '.##.', '####', '####', '.##.',
    ])
    print(f'\nXPM from array: {icon.w}x{icon.h}, {SDL3.GetPixelFormatName(icon.format)}')

SDL3.Quit()
