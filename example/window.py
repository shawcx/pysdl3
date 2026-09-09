#!/usr/bin/env python3

# Window-state playground. Keys:
#   b  toggle border        g  toggle mouse grab      o  cycle opacity
#   t  toggle always-on-top f  flash the window       m  message box
#   i  print display / Vulkan info
# Esc or close to quit.

import SDL3

SDL3.Init(SDL3.INIT_VIDEO)

window = SDL3.Window('window flags', (480, 320), flags=SDL3.WINDOW_RESIZABLE)
renderer = window.CreateRenderer()

bordered = True
grabbed = False
on_top = False
opacity_steps = [1.0, 0.75, 0.5]
opacity = 0


def safe(label, fn):
    try:
        fn()
        print(f'  {label}: ok')
    except SDL3.error as exc:
        print(f'  {label}: {exc}')


def show_info():
    d = window.GetWindowDisplayIndex()
    print(f'display {d}: {SDL3.GetDisplayName(d)!r}')
    print(f'  bounds        {SDL3.GetDisplayBounds(d)}')
    print(f'  usable bounds {SDL3.GetDisplayUsableBounds(d)}')
    print(f'  orientation   {SDL3.GetDisplayOrientation(d)}')
    print(f'  modes         {SDL3.GetNumDisplayModes(d)}  (current {SDL3.GetCurrentDisplayMode(d)})')
    print(f'  window flags  {window.GetWindowFlags():#010x}')
    print(f'  borders       {window.GetWindowBordersSize()}')
    locales = getattr(SDL3, 'GetPreferredLocales', list)()
    print(f'  locales       {locales}')
    try:
        SDL3.Vulkan_LoadLibrary()
        print(f'  vulkan exts   {SDL3.Vulkan_GetInstanceExtensions()}')
        SDL3.Vulkan_UnloadLibrary()
    except SDL3.error as exc:
        print(f'  vulkan        unavailable ({exc})')


def redraw():
    renderer.SetRenderDrawColor(30, 34, 48, 255)
    renderer.Clear()
    renderer.SetRenderDrawColor(90, 160, 240, 255)
    renderer.DrawRect((20, 20, *[c - 40 for c in window.GetWindowSize()]))
    renderer.Present()


redraw()
running = True
while running:
    event = SDL3.WaitEvent()
    if event is None:
        continue
    kind, data = event

    if kind == SDL3.EVENT_QUIT:
        running = False

    elif SDL3.EVENT_WINDOW_FIRST <= kind <= SDL3.EVENT_WINDOW_LAST:
        redraw()

    elif kind == SDL3.EVENT_KEY_DOWN:
        sym = data[2]
        if sym == SDL3.K_ESCAPE:
            running = False
        elif sym == SDL3.K_B:
            bordered = not bordered
            window.SetWindowBordered(bordered)
        elif sym == SDL3.K_G:
            grabbed = not grabbed
            window.SetWindowGrab(grabbed)
            print(f'grab -> {window.GetWindowGrab()}')
        elif sym == SDL3.K_O:
            opacity = (opacity + 1) % len(opacity_steps)
            safe(f'opacity {opacity_steps[opacity]}',
                 lambda: window.SetWindowOpacity(opacity_steps[opacity]))
        elif sym == SDL3.K_T and hasattr(window, 'SetWindowAlwaysOnTop'):
            on_top = not on_top
            window.SetWindowAlwaysOnTop(on_top)
            print(f'always-on-top -> {on_top}')
        elif sym == SDL3.K_F and hasattr(window, 'FlashWindow'):
            safe('flash', lambda: window.FlashWindow(SDL3.FLASH_BRIEFLY))
        elif sym == SDL3.K_M:
            choice = SDL3.ShowMessageBox({
                'window': window,
                'flags': SDL3.MESSAGEBOX_INFORMATION,
                'title': 'pysdl2',
                'message': 'Keep going?',
                'buttons': [
                    (0, 'Quit', SDL3.MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT),
                    (1, 'Continue', SDL3.MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT),
                ],
            })
            print(f'message box -> {choice}')
            if choice == 0:
                running = False
        elif sym == SDL3.K_I:
            show_info()

SDL3.Quit()
