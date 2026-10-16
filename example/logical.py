#!/usr/bin/env python3

# Logical-resolution rendering: a fixed 160x120 canvas scaled to a resizable
# window by SDL3's logical presentation. Shows mouse picking with
# RenderWindowToLogical, a RenderGeometryRaw colour wheel fed straight from
# arrays, a streaming texture drawn through LockToSurface, and the window's
# pixel size (differs on high-DPI screens).
#
#   V toggles vsync, P cycles the presentation mode, Esc or close quits.
#   --frames N exits after N frames (with SDL_VIDEO_DRIVER=dummy it runs headless).

import array
import math
import sys
import SDL3

LOGICAL_W, LOGICAL_H = 160, 120
MODES = [
    ('letterbox', SDL3.LOGICAL_PRESENTATION_LETTERBOX),
    ('overscan', SDL3.LOGICAL_PRESENTATION_OVERSCAN),
    ('stretch', SDL3.LOGICAL_PRESENTATION_STRETCH),
    ('integer scale', SDL3.LOGICAL_PRESENTATION_INTEGER_SCALE),
]
frames_left = int(sys.argv[sys.argv.index('--frames') + 1]) if '--frames' in sys.argv else None

SDL3.Init(SDL3.INIT_VIDEO)
window, renderer = SDL3.CreateWindowAndRenderer('logical', (640, 480), SDL3.WINDOW_RESIZABLE)
assert window.GetRenderer() is renderer
print('renderer', renderer.GetRendererInfo()[0])
print('window size', window.GetWindowSize(), 'pixels', window.GetWindowSizeInPixels())

mode = 0


def present(index):
    name, value = MODES[index]
    renderer.SetRenderLogicalPresentation(LOGICAL_W, LOGICAL_H, value)
    print('presentation', name, 'lands in', renderer.GetRenderLogicalPresentationRect())


present(mode)

# Colour wheel: centre vertex plus a ring, drawn as an indexed triangle fan.
# Positions are float32 pairs, colours float32 RGBA (SDL_FColor, 0..1).
SEGMENTS = 24
xy = array.array('f', [40.0, 60.0])
rgba = array.array('f', [1.0, 1.0, 1.0, 1.0])
for i in range(SEGMENTS):
    angle = 2 * math.pi * i / SEGMENTS
    xy.extend([40 + 30 * math.cos(angle), 60 + 30 * math.sin(angle)])
    rgba.extend([0.5 + 0.5 * math.cos(angle + k * 2.094) for k in range(3)] + [1.0])
indices = array.array('H')
for i in range(SEGMENTS):
    indices.extend([0, 1 + i, 1 + (i + 1) % SEGMENTS])

# A streaming texture redrawn every frame through a Surface.
stripes = SDL3.Texture(renderer, SDL3.PIXELFORMAT_RGBA32, SDL3.TEXTUREACCESS_STREAMING, (32, 32))

vsync = False
cursor = (LOGICAL_W / 2, LOGICAL_H / 2)
frame = 0
running = True
while running:
    while (event := SDL3.PollEvent()) is not None:
        kind, data = event
        if kind == SDL3.EVENT_QUIT:
            running = False
        elif kind == SDL3.EVENT_KEY_DOWN and data[2] == SDL3.K_ESCAPE:
            running = False
        elif kind == SDL3.EVENT_KEY_DOWN and data[2] == SDL3.K_V:
            vsync = not vsync
            try:
                renderer.SetVSync(1 if vsync else SDL3.RENDERER_VSYNC_DISABLED)
            except SDL3.error as exc:
                print('vsync', exc)
            print('vsync', renderer.GetVSync())
        elif kind == SDL3.EVENT_KEY_DOWN and data[2] == SDL3.K_P:
            mode = (mode + 1) % len(MODES)
            present(mode)
        elif kind == SDL3.EVENT_MOUSE_MOTION:
            # Mouse events arrive in window coordinates; map them onto the canvas.
            cursor = renderer.RenderWindowToLogical((data[1], data[2]))
        elif kind == SDL3.EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            print('pixels', window.GetWindowSizeInPixels(),
                  'canvas lands in', renderer.GetRenderLogicalPresentationRect())

    surface = stripes.LockToSurface()
    for y in range(0, 32, 4):
        shade = (frame * 4 + y * 8) % 256
        surface.FillRect((0, y, 32, 4), (shade, 64, 255 - shade, 255))
    stripes.Unlock()  # uploads; `surface` is now an empty 0x0 stand-in

    renderer.SetRenderDrawColor(16, 16, 24, 255)
    renderer.Clear()
    renderer.RenderGeometryRaw(None, xy, rgba, indices=indices, index_size=2)
    renderer.Copy(stripes, None, (100, 44, 32, 32))

    renderer.SetRenderDrawColor(255, 255, 255, 255)
    renderer.DrawRect((0, 0, LOGICAL_W, LOGICAL_H))
    cx, cy = cursor
    renderer.DrawLine(cx - 4, cy, cx + 4, cy)
    renderer.DrawLine(cx, cy - 4, cx, cy + 4)
    renderer.Present()

    frame += 1
    if frames_left is not None:
        frames_left -= 1
        running = running and frames_left > 0
    elif not vsync:
        SDL3.Delay(16)

# Back from canvas to window coordinates, e.g. to place a native widget.
print('canvas centre is window', renderer.RenderLogicalToWindow((LOGICAL_W / 2, LOGICAL_H / 2)))
SDL3.Quit()
