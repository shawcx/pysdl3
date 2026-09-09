#!/usr/bin/env python3

# Software-surface compositing: build an image with fills and alpha blits,
# convert it, and write it out. No window required.
#
#   python3 example/surface.py [out.png]

import sys
import SDL3

SDL3.Init(SDL3.INIT_VIDEO)

W, H = 320, 240
out = sys.argv[1] if len(sys.argv) > 1 else 'surface.png'

canvas = SDL3.CreateRGBSurfaceWithFormat((W, H), SDL3.PIXELFORMAT_RGBA8888)
canvas.FillRect(None, (24, 28, 40, 255))

# a checkerboard drawn with FillRects
squares = [(x, y, 20, 20)
           for y in range(0, H, 20)
           for x in range(0, W, 20)
           if (x // 20 + y // 20) % 2 == 0]
canvas.FillRects(squares, (36, 42, 60, 255))

# a translucent stamp, blitted several times with different alpha
stamp = SDL3.CreateRGBSurfaceWithFormat((80, 80), SDL3.PIXELFORMAT_RGBA8888)
stamp.FillRect(None, (0, 0, 0, 0))
stamp.FillRect((10, 10, 60, 60), (240, 120, 40, 255))
stamp.SetBlendMode(SDL3.BLENDMODE_BLEND)

for i in range(5):
    stamp.SetAlphaMod(60 + i * 45)
    canvas.Blit(stamp, None, (20 + i * 45, 30 + i * 20))

# a scaled copy of the whole canvas, dropped into the corner
thumb = canvas.Duplicate()
canvas.BlitScaled(thumb, None, (W - 90, H - 70, 80, 60))

# round-trip through a 24-bit format, then save
canvas.Convert(SDL3.PIXELFORMAT_RGB24).SavePNG(out)
print('wrote', out, f'({W}x{H})')

SDL3.Quit()
