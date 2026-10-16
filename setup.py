#!/usr/bin/env python3

import glob
import os
import shlex
import subprocess

import setuptools

defines = [('_REENTRANT', None), ('_GNU_SOURCE', '1')]
libs = ['SDL3', 'SDL3_image']
source_files = glob.glob('./src/*.c')

# Homebrew installs SDL3 keg-only-ish: the .pc files live under each formula's
# own prefix, so seed PKG_CONFIG_PATH with the usual spots before probing.
_BREW_PKGCONFIG = [
    '/opt/homebrew/opt/sdl3/lib/pkgconfig',
    '/opt/homebrew/opt/sdl3_image/lib/pkgconfig',
    '/opt/homebrew/lib/pkgconfig',
    '/usr/local/opt/sdl3/lib/pkgconfig',
    '/usr/local/opt/sdl3_image/lib/pkgconfig',
    '/usr/local/lib/pkgconfig',
]


def _probe_sdl():
    '''Ask pkg-config where SDL3 lives; fall back to the common Homebrew and
    /usr/local prefixes so a bare `pip install .` still works. (SDL3 ships no
    sdl3-config script, unlike SDL2.)'''
    include_dirs = []
    library_dirs = []

    env = dict(os.environ)
    existing = env.get('PKG_CONFIG_PATH', '')
    env['PKG_CONFIG_PATH'] = os.pathsep.join(
        [p for p in _BREW_PKGCONFIG if os.path.isdir(p)] +
        ([existing] if existing else []))

    try:
        out = subprocess.check_output(
            ['pkg-config', '--cflags', '--libs', 'sdl3', 'sdl3-image'],
            text=True, env=env)
    except (OSError, subprocess.CalledProcessError):
        out = ''

    for token in shlex.split(out):
        if token.startswith('-I'):
            include_dirs.append(token[2:])
        elif token.startswith('-L'):
            library_dirs.append(token[2:])

    include_dirs += ['/usr/local/include', '/opt/homebrew/include']
    library_dirs += ['/usr/local/lib', '/opt/homebrew/lib']
    return include_dirs, library_dirs


include_dirs, library_dirs = _probe_sdl()

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
