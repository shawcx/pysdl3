#!/usr/bin/env python3

import SDL3

SDL3.Init()

print('Platform:',  SDL3.GetPlatform())
print('SDL3 Version: %d.%d.%d' % SDL3.Version())
print()
print('CPU Count:  ', SDL3.GetCPUCount())
print('Cache Line: ', SDL3.GetCPUCacheLineSize())
for name in ('HasAVX', 'HasAVX2', 'HasAltiVec', 'HasSSE', 'HasSSE2',
             'HasSSE3', 'HasSSE41', 'HasSSE42', 'HasNEON'):
    print(f'{name + ":":12}', getattr(SDL3, name)())
print()

drivers = SDL3.GetVideoDrivers()
current = SDL3.GetCurrentVideoDriver()
print('Number of drivers:', len(drivers))
for driver in drivers:
    print('-', driver, '(current)' if current == driver else '')
print()

displays = SDL3.GetDisplays()
print('Number of displays:', len(displays))
for display in displays:
    fmt, width, height, rate = SDL3.GetCurrentDisplayMode(display)
    print('-', f'{SDL3.GetDisplayName(display)}: {width}x{height} @ {rate:g}Hz')
    fmt, width, height, rate = SDL3.GetDesktopDisplayMode(display)
    print('-', f'Desktop Display Mode: {width}x{height} @ {rate:g}Hz')
print()

numberOfRenderers = SDL3.GetNumRenderDrivers()
print('Number of renderers:', numberOfRenderers)
for idx in range(numberOfRenderers):
    print('-', SDL3.GetRenderDriverInfo(idx))
print()

SDL3.Quit()
