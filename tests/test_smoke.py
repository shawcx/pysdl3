'''Headless port of example/info.py: the module loads and the query API works.'''


def test_compiled_and_linked_versions(sdl):
    assert sdl.Version()[0] == 3
    assert sdl.GetVersion()[0] == 3
    assert isinstance(sdl.GetRevision(), str)


def test_platform_and_cpu(sdl):
    assert sdl.GetPlatform()
    assert sdl.GetCPUCount() >= 1
    assert sdl.GetCPUCacheLineSize() >= 0
    for name in ('HasAVX', 'HasAVX2', 'HasSSE', 'HasSSE2', 'HasNEON'):
        assert isinstance(getattr(sdl, name)(), bool)


def test_video_driver_is_dummy(sdl):
    assert 'dummy' in sdl.GetVideoDrivers()
    assert sdl.GetCurrentVideoDriver() == 'dummy'


def test_displays_and_renderers(sdl):
    displays = sdl.GetDisplays()
    assert len(displays) >= 1
    fmt, w, h, rate = sdl.GetDesktopDisplayMode(displays[0])
    assert w > 0 and h > 0
    assert sdl.GetNumRenderDrivers() >= 1
