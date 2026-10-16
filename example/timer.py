#!/usr/bin/env python3

# A fixed-rate animation driven by SDL3.Timer. The timer callback runs on SDL's
# timer thread, so it does not touch the renderer - it just pushes a REDRAW
# event that the main loop handles. The title shows battery / power state.
# Esc or close to quit.

import math
import SDL3

SDL3.Init(SDL3.INIT_VIDEO | SDL3.INIT_EVENTS)

W, H = 400, 400
window = SDL3.Window('timer', (W, H))
renderer = window.CreateRenderer()

REDRAW = SDL3.RegisterEvents(1)

frame = 0


def on_tick(interval):
    # 60 Hz; return the interval to keep going, 0 to stop.
    SDL3.PushEvent(REDRAW)
    return interval


timer = SDL3.Timer(1000 // 60, on_tick)

POWER = {SDL3.POWERSTATE_UNKNOWN: '?', SDL3.POWERSTATE_ON_BATTERY: 'battery',
         SDL3.POWERSTATE_NO_BATTERY: 'AC', SDL3.POWERSTATE_CHARGING: 'charging',
         SDL3.POWERSTATE_CHARGED: 'charged'}


def render():
    global frame
    frame += 1
    t = frame / 60.0

    renderer.SetRenderDrawColor(18, 18, 26, 255)
    renderer.Clear()

    cx, cy = W / 2, H / 2
    for i in range(12):
        a = t + i * math.tau / 12
        r = 150 + 30 * math.sin(t * 2 + i)
        x, y = cx + r * math.cos(a), cy + r * math.sin(a)
        shade = int(128 + 127 * math.sin(t + i))
        renderer.SetRenderDrawColor(shade, 200 - shade // 2, 255 - shade, 255)
        renderer.FillRect((x - 8, y - 8, 16, 16))

    # clock hand
    renderer.SetRenderDrawColor(240, 240, 240, 255)
    renderer.DrawLine(cx, cy, cx + 120 * math.cos(t), cy + 120 * math.sin(t))

    renderer.Present()

    if frame % 30 == 0:
        state, secs, pct = SDL3.GetPowerInfo()
        window.SetWindowTitle(f'timer  |  {frame} frames  |  {POWER[state]} {pct}%')


running = True
while running:
    event = SDL3.WaitEvent()
    if event is None:
        continue
    kind, data = event

    if kind == REDRAW:
        render()
    elif kind == SDL3.EVENT_QUIT:
        running = False
    elif kind == SDL3.EVENT_KEY_DOWN and data[2] == SDL3.K_ESCAPE:
        running = False

timer.Remove()
SDL3.Quit()
