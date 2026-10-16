'''Shared pytest setup: run headless, import the freshly built extension.'''

import importlib.machinery
import os
import pathlib
import subprocess
import sys

import pytest

# Drivers that need no display, sound card, or window server.
os.environ.setdefault('SDL_VIDEODRIVER', 'dummy')
os.environ.setdefault('SDL_AUDIODRIVER', 'dummy')

_ROOT = pathlib.Path(__file__).resolve().parent.parent


def _built_module():
    # build/ keeps a lib.* dir per platform (one per macOS version, say), so
    # take the newest extension this interpreter can import, not the first.
    built = [p for suffix in importlib.machinery.EXTENSION_SUFFIXES
             for p in (_ROOT / 'build').glob('lib*/SDL3' + suffix)]
    return max(built, key=lambda p: p.stat().st_mtime, default=None)


_module = _built_module()
_newest_source = max(p.stat().st_mtime for p in (_ROOT / 'src').glob('*.[ch]'))
if _module is None or _module.stat().st_mtime < _newest_source:
    subprocess.run([sys.executable, 'setup.py', 'build'], cwd=_ROOT, check=True)
    _module = _built_module()

if _module is not None and str(_module.parent) not in sys.path:
    sys.path.insert(0, str(_module.parent))


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
