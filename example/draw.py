#!/usr/bin/env python3

# Primitives, blend modes, and a render-to-texture pass. Esc or close to quit.

import math
import SDL3

SDL3.Init(SDL3.INIT_VIDEO)

WIDTH, HEIGHT = 640, 480

window = SDL3.Window('draw', (WIDTH, HEIGHT), flags=SDL3.WINDOW_RESIZABLE)
renderer = window.CreateRenderer()
renderer.SetVSync(1)

print('renderer:', renderer.GetRendererInfo()[0])
print('output:  ', renderer.GetRendererOutputSize())

# A little checker texture we can draw into once.
sprite = SDL3.Texture(renderer, SDL3.PIXELFORMAT_RGBA8888, SDL3.TEXTUREACCESS_TARGET, (64, 64))
sprite.SetBlendMode(SDL3.BLENDMODE_BLEND)
renderer.SetRenderTarget(sprite)
for y in range(8):
    for x in range(8):
        renderer.SetRenderDrawColor(*(((x + y) % 2) and (240, 200, 40) or (40, 120, 240)), 255)
        renderer.FillRect((x * 8, y * 8, 8, 8))
renderer.SetRenderTarget(None)

renderer.SetRenderDrawBlendMode(SDL3.BLENDMODE_BLEND)

frame = 0
running = True
while running:
    while True:
        event = SDL3.PollEvent()
        if event is None:
            break
        kind, data = event
        if kind == SDL3.EVENT_QUIT:
            running = False
        elif kind == SDL3.EVENT_KEY_DOWN and data[2] == SDL3.K_ESCAPE:
            running = False

    frame += 1
    t = frame / 60.0

    renderer.SetRenderDrawColor(16, 16, 24, 255)
    renderer.Clear()

    # grid
    renderer.SetRenderDrawColor(40, 40, 60, 255)
    for x in range(0, WIDTH, 40):
        renderer.DrawLine(x, 0, x, HEIGHT)
    for y in range(0, HEIGHT, 40):
        renderer.DrawLine(0, y, WIDTH, y)

    # a fan of points
    renderer.SetRenderDrawColor(120, 220, 160, 255)
    renderer.DrawPoints([(WIDTH / 2 + 120 * math.cos(a / 12 + t),
                          HEIGHT / 2 + 120 * math.sin(a / 12 + t)) for a in range(240)])

    # translucent rectangles
    for i in range(6):
        renderer.SetRenderDrawColor(220, 80, 80, 90)
        renderer.FillRect((60 + i * 40, 60 + 30 * math.sin(t + i), 80, 80))

    # the sprite, orbiting and spinning
    dst = (WIDTH / 2 - 32 + 160 * math.cos(t), HEIGHT / 2 - 32 + 90 * math.sin(t * 1.3), 64, 64)
    renderer.CopyExF(sprite, None, dst, math.degrees(t) % 360)

    # triangle via geometry (SDL >= 2.0.18)
    if hasattr(renderer, 'RenderGeometry'):
        cx, cy = WIDTH - 120, HEIGHT - 120
        renderer.RenderGeometry(None, [
            ((cx, cy - 50), (255, 80, 80, 255)),
            ((cx + 50, cy + 40), (80, 255, 80, 255)),
            ((cx - 50, cy + 40), (80, 80, 255, 255)),
        ])

    renderer.Present()

SDL3.Quit()
