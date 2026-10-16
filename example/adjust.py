#!/usr/bin/env python3

# For LCD monitors using a VGA connection run this program then "auto-adjust" the monitor

import SDL3

# Initialize SDL3
SDL3.Init(SDL3.INIT_VIDEO)

windows = []
for display in SDL3.GetDisplays():
    x,y,w,h = SDL3.GetDisplayBounds(display)
    window = SDL3.Window("Adjust", size=(w,h), position=(x,y), flags=SDL3.WINDOW_FULLSCREEN)
    windows.append((window, w, h))

# Hide the cursor
SDL3.ShowCursor(False)

white = b'\xff\xff\xff\x00'
black = b'\x00\x00\x00\x00'

for window,w,h in windows:
    surface = window.GetWindowSurface()
    surface.LockSurface()
    surface.pixels = ((white + black) * int(w/2) + (black + white) * int(w/2)) * int(h/2)
    surface.UnlockSurface()
    window.UpdateWindowSurface()

while True:
    event = SDL3.WaitEvent()
    if event:
        if SDL3.EVENT_QUIT == event[0]:
            break
        if SDL3.EVENT_KEY_UP == event[0]:
            break

# Show the cursor
SDL3.ShowCursor(True)
SDL3.Quit()
