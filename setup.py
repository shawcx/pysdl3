#!/usr/bin/env python3

import glob
import os
import shlex
import subprocess

import setuptools

defines = [('_REENTRANT', None), ('_GNU_SOURCE', '1')]
libs = ['SDL3', 'SDL3_image']
source_files = glob.glob('./src/*.c')

# Optional satellite libraries, built in only when found (pkg-config, else
# the header <library>/SDL_<name>.h). Set the env var (e.g. PYSDL_TTF=0) to
# leave one out even when it is installed.
OPTIONAL = [
    # pkg-config name, library, define, opt-out env var
    ('sdl3-ttf', 'SDL3_ttf', 'PYSDL_HAVE_TTF', 'PYSDL_TTF'),
]

# Homebrew installs SDL3 keg-only-ish: the .pc files live under each formula's
# own prefix, so seed PKG_CONFIG_PATH with the usual spots before probing.
_BREW_PKGCONFIG = [
    '/opt/homebrew/opt/sdl3/lib/pkgconfig',
    '/opt/homebrew/opt/sdl3_image/lib/pkgconfig',
    '/opt/homebrew/opt/sdl3_ttf/lib/pkgconfig',
    '/opt/homebrew/lib/pkgconfig',
    '/usr/local/opt/sdl3/lib/pkgconfig',
    '/usr/local/opt/sdl3_image/lib/pkgconfig',
    '/usr/local/opt/sdl3_ttf/lib/pkgconfig',
    '/usr/local/lib/pkgconfig',
]


def _pkg_config(*packages, quiet=False):
    '''-> (include_dirs, library_dirs) for `packages` from pkg-config, or
    None when one is missing (or pkg-config is).'''
    env = dict(os.environ)
    existing = env.get('PKG_CONFIG_PATH', '')
    env['PKG_CONFIG_PATH'] = os.pathsep.join(
        [p for p in _BREW_PKGCONFIG if os.path.isdir(p)] +
        ([existing] if existing else []))

    try:
        out = subprocess.check_output(
            ['pkg-config', '--cflags', '--libs', *packages], text=True, env=env,
            stderr=subprocess.DEVNULL if quiet else None)
    except (OSError, subprocess.CalledProcessError):
        return None

    tokens = shlex.split(out)
    return ([t[2:] for t in tokens if t.startswith('-I')],
            [t[2:] for t in tokens if t.startswith('-L')])


def _probe_sdl():
    '''Ask pkg-config where SDL3 lives; fall back to the common Homebrew and
    /usr/local prefixes so a bare `pip install .` still works. (SDL3 ships no
    sdl3-config script, unlike SDL2.)'''
    include_dirs, library_dirs = _pkg_config('sdl3', 'sdl3-image') or ([], [])
    include_dirs += ['/usr/local/include', '/opt/homebrew/include']
    library_dirs += ['/usr/local/lib', '/opt/homebrew/lib']
    return include_dirs, library_dirs


def _probe_optional(include_dirs, library_dirs):
    '''Link each OPTIONAL library that is installed and define its macro.'''
    for pkg, lib, define, env in OPTIONAL:
        if os.environ.get(env) == '0':
            continue
        found = _pkg_config(pkg, quiet=True)
        if found is not None:
            include_dirs += [d for d in found[0] if d not in include_dirs]
            library_dirs += [d for d in found[1] if d not in library_dirs]
        else:
            header = os.path.join(lib, 'SDL_%s.h' % lib.split('_', 1)[1])  # SDL3_ttf/SDL_ttf.h
            if not any(os.path.isfile(os.path.join(d, header))
                       for d in include_dirs + ['/usr/include']):
                continue
        libs.append(lib)
        defines.append((define, '1'))


include_dirs, library_dirs = _probe_sdl()
_probe_optional(include_dirs, library_dirs)

setuptools.setup(
    name             = 'SDL3',
    version          = '0.3.0',
    author           = 'Matthew Shaw',
    author_email     = 'mshaw.cx@gmail.com',
    url              = 'https://github.com/shawcx/pysdl3',
    license          = 'MIT',
    description      = 'Python3 bindings for SDL3',
    long_description = open('README.rst').read(),
    python_requires  = '>=3.10',
    ext_modules = [
        setuptools.Extension(
            'SDL3',
            source_files,
            depends       = glob.glob('./src/*.h'),
            define_macros = defines,
            include_dirs  = include_dirs,
            library_dirs  = library_dirs,
            libraries     = libs
            )
        ],
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'Programming Language :: Python :: 3',
        ]
    )
