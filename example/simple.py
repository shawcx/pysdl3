#!/usr/bin/env python3

import sys
import time
import SDL3

imagePath = sys.argv[1]

SDL3.Init()

surface = SDL3.LoadImage(imagePath)
window = SDL3.Window(imagePath, (surface.w,surface.h))

window.SetWindowIcon(surface)

print('%s: %dx%d @ %dbpp' % (imagePath, surface.w, surface.h, surface.bpp))

renderer = window.CreateRenderer()

#tex2 = renderer.LoadTexture(sys.argv[2])
#for x in range(0,200,10):
#    renderer.SetRenderDrawColor(128,128,x)
#    renderer.Clear()
#    renderer.Present()
#    time.sleep(0.1)

texture = renderer.CreateTextureFromSurface(surface)
del surface

renderer.Clear()
renderer.Copy(texture)
renderer.Present()

quit = False
while not quit:
    while True:
        result = SDL3.PollEvent()
        if result is None:
            break

        event,data = result

        if SDL3.EVENT_QUIT == event:
            quit = True
            break

        if SDL3.EVENT_KEY_DOWN == event:
            # quit on escape
            if SDL3.K_ESCAPE == data[2]:
                quit = True
                break
            if SDL3.K_H == data[2]:
                window.HideWindow()
                break
            if SDL3.K_M == data[2]:
                window.MinimizeWindow()
                break
#            # take a screenshot on c
#            if SDL3.K_C == data[2]:
#                self.Capture()
#            # display info on i
#            if SDL3.K_I == data[2]:
#                self.fnInfo()

del texture
del renderer
del window

SDL3.Quit()
