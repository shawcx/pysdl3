'''The bulk constant table from src/_constants.c is present and integer-typed.

Names follow SDL3 (the SDL_ prefix dropped): SDL_EVENT_KEY_DOWN -> EVENT_KEY_DOWN,
SDL_KMOD_CTRL -> KMOD_CTRL, SDLK_A -> K_A.
'''

import pytest

SAMPLE = [
    'INIT_VIDEO', 'INIT_EVERYTHING',
    'EVENT_QUIT', 'EVENT_KEY_DOWN', 'EVENT_KEY_UP', 'EVENT_MOUSE_MOTION',
    'EVENT_WINDOW_RESIZED', 'EVENT_USER', 'EVENT_FIRST', 'EVENT_LAST',
    'WINDOW_OPENGL', 'WINDOW_FULLSCREEN', 'WINDOW_RESIZABLE',
    'RENDERER_VSYNC_DISABLED', 'LOGICAL_PRESENTATION_LETTERBOX',
    'TEXTUREACCESS_STREAMING', 'TEXTUREACCESS_TARGET',
    'FLIP_NONE', 'FLIP_HORIZONTAL',
    'K_ESCAPE', 'K_SPACE', 'K_A', 'K_UP',
    'KMOD_LSHIFT', 'KMOD_CTRL',
    'SCANCODE_A', 'SCANCODE_RETURN',
    'PIXELFORMAT_RGBA8888', 'PIXELFORMAT_XRGB8888',
    'GL_CONTEXT_MAJOR_VERSION', 'GL_DOUBLEBUFFER',
    'AUDIO_S16', 'AUDIO_F32',
    'GAMEPAD_BUTTON_SOUTH', 'GAMEPAD_AXIS_LEFTX',
    'SYSTEM_CURSOR_DEFAULT',
]

# Constants that existed under SDL2 and must NOT reappear (removed by SDL3).
GONE = [
    'ENABLE', 'DISABLE', 'QUERY',
    'KEYDOWN', 'WINDOWEVENT', 'MULTIGESTURE', 'DOLLARGESTURE',
    'WINDOW_SHOWN', 'WINDOW_FULLSCREEN_DESKTOP',
    'RENDERER_ACCELERATED', 'RENDERER_PRESENTVSYNC',
    'AUDIO_ALLOW_ANY_CHANGE', 'AUDIO_F32SYS',
    'INIT_NOPARACHUTE', 'INIT_TIMER',
    'IMG_INIT_PNG',
]


@pytest.mark.parametrize('name', SAMPLE)
def test_constant_is_int(sdl, name):
    assert isinstance(getattr(sdl, name), int)


@pytest.mark.parametrize('name', GONE)
def test_constant_removed(sdl, name):
    assert not hasattr(sdl, name)


def test_error_type_exposed(sdl):
    assert issubclass(sdl.error, Exception)
