#!/usr/bin/env python3

# Rect geometry, live. A rect follows the mouse; its intersection with the
# fixed rect is filled, their union is outlined, and the box enclosing the
# recent mouse trail is drawn dashed. Esc or close to quit.

import collections
import SDL3

SDL3.Init(SDL3.INIT_VIDEO)

W, H = 640, 480
window = SDL3.Window('rects', (W, H))
renderer = window.CreateRenderer()

FIXED = (200, 150, 240, 180)
MOVING_SIZE = (160, 120)
trail = collections.deque(maxlen=90)
mouse = (W // 2, H // 2)


def frame():
    mx, my = mouse
    moving = (mx - MOVING_SIZE[0] // 2, my - MOVING_SIZE[1] // 2, *MOVING_SIZE)

    renderer.SetRenderDrawColor(16, 18, 26, 255)
    renderer.Clear()

    # union outline
    if union := SDL3.UnionRect(FIXED, moving):
        renderer.SetRenderDrawColor(70, 70, 110, 255)
        renderer.DrawRect(union)

    # the two rects
    renderer.SetRenderDrawColor(90, 160, 240, 255)
    renderer.DrawRect(FIXED)
    renderer.SetRenderDrawColor(240, 170, 90, 255)
    renderer.DrawRect(moving)

    # intersection, filled
    if hit := SDL3.IntersectRect(FIXED, moving):
        renderer.SetRenderDrawColor(120, 220, 160, 130)
        renderer.SetRenderDrawBlendMode(SDL3.BLENDMODE_BLEND)
        renderer.FillRect(hit)

    # box enclosing the mouse trail
    if len(trail) > 1 and (box := SDL3.EnclosePoints(list(trail))):
        renderer.SetRenderDrawColor(200, 90, 200, 255)
        x, y, w, h = box
        for i in range(0, w, 12):
            renderer.DrawLine(x + i, y, x + min(i + 6, w), y)
            renderer.DrawLine(x + i, y + h, x + min(i + 6, w), y + h)
        for i in range(0, h, 12):
            renderer.DrawLine(x, y + i, x, y + min(i + 6, h))
            renderer.DrawLine(x + w, y + i, x + w, y + min(i + 6, h))

    # a line clipped to the fixed rect
    clipped = SDL3.IntersectRectAndLine(FIXED, (0, 0, mx, my))
    if clipped:
        renderer.SetRenderDrawColor(255, 255, 255, 255)
        renderer.DrawLine(*clipped)

    renderer.Present()


frame()
running = True
while running:
    event = SDL3.WaitEvent()
    if event is None:
        continue
    kind, data = event

    if kind == SDL3.EVENT_QUIT:
        running = False
    elif kind == SDL3.EVENT_KEY_DOWN and data[2] == SDL3.K_ESCAPE:
        running = False
    elif kind == SDL3.EVENT_MOUSE_MOTION:
        mouse = (data[1], data[2])
        trail.append(mouse)
        frame()

SDL3.Quit()
