'''Shared pytest setup: run headless, import the freshly built extension.'''

import glob
import os
import pathlib
import subprocess
import sys

import pytest

# Drivers that need no display, sound card, or window server.
os.environ.setdefault('SDL_VIDEODRIVER', 'dummy')
os.environ.setdefault('SDL_AUDIODRIVER', 'dummy')

_ROOT = pathlib.Path(__file__).resolve().parent.parent


def _built_module_dir():
    for path in sorted(glob.glob(str(_ROOT / 'build' / 'lib*'))):
        if glob.glob(os.path.join(path, 'SDL3*')):
            return path
    return None


_moddir = _built_module_dir()
if _moddir is None:
    subprocess.run([sys.executable, 'setup.py', 'build'], cwd=_ROOT, check=True)
    _moddir = _built_module_dir()

if _moddir is not None and _moddir not in sys.path:
    sys.path.insert(0, _moddir)


@pytest.fixture(scope='session')
def sdl():
    '''SDL initialised once for the whole test session.'''
    import SDL3
    SDL3.Init(SDL3.INIT_VIDEO | SDL3.INIT_AUDIO | SDL3.INIT_EVENTS)
    yield SDL3
    SDL3.Quit()


@pytest.fixture
def window(sdl):
    return sdl.Window('pysdl2-test', (64, 48))


@pytest.fixture
def renderer(sdl, window):
    try:
        return window.CreateRenderer()
    except sdl.error:
        pytest.skip('no renderer available under this driver')


@pytest.fixture
def texture(sdl, renderer):
    return renderer.CreateTextureFromSurface(sdl.CreateRGBSurface((8, 8)))
