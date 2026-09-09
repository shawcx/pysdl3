#!/usr/bin/env python3
'''Report SDL3 constants that the binding does not expose yet.

Scans the installed SDL3 headers for enum members and simple integer #defines
(names SDL_*, SDLK_*) and compares them with the symbols src/_constants.c already
registers. Use it when wiring up a new subsystem to see what is still missing.

    tools/gen_constants.py                  # missing symbols grouped by header
    tools/gen_constants.py --emit SCANCODE  # PyModule_AddIntConstant lines to paste

SDL3 ships no sdl3-config script; this probes `pkg-config sdl3` (seeding
PKG_CONFIG_PATH with the Homebrew keg dirs) and falls back to the usual prefixes.

The --emit output still needs a human to drop it into _constants.c (usually
inside a new section, sometimes guarded with #ifdef for portability).
'''

import argparse
import glob
import os
import re
import shlex
import subprocess
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_CONSTANTS_C = os.path.join(_HERE, '..', 'src', '_constants.c')

_NAME = r'(SDL_[A-Z0-9_]+|SDLK_[A-Z0-9_]+)'
_DEFINE_RE = re.compile(r'^\s*#\s*define\s+' + _NAME + r'\s+(.+?)\s*$')
_ENUM_MEMBER_RE = re.compile(r'\b' + _NAME + r'\b')
_REGISTERED_RE = re.compile(r'PyModule_AddIntConstant\s*\(\s*module\s*,\s*"[^"]+"\s*,\s*([A-Za-z0-9_]+)\s*\)')
_NUMERIC_BODY_RE = re.compile(r"^[0-9a-fA-FxXuUlL'\"\s()|+\-<>*/~.,]*$")
_REF_BODY_RE = re.compile(r'\b(SDL_|SDLK_)[A-Z0-9_]+\b')

# Compiler / build plumbing - never real runtime constants.
_SKIP_HEADERS = {
    'begin_code.h', 'close_code.h', 'SDL_assert.h', 'SDL_endian.h', 'SDL_main.h',
    'SDL_platform.h', 'SDL_stdinc.h', 'SDL_cpuinfo.h', 'SDL_config.h',
    'SDL_egl.h', 'SDL_opengl.h', 'SDL_opengles.h', 'SDL_opengles2.h', 'SDL_opengl_glext.h',
}


_BREW_PKGCONFIG = [
    '/opt/homebrew/opt/sdl3/lib/pkgconfig',
    '/opt/homebrew/lib/pkgconfig',
    '/usr/local/opt/sdl3/lib/pkgconfig',
    '/usr/local/lib/pkgconfig',
]


def sdl_include_dir():
    env = dict(os.environ)
    existing = env.get('PKG_CONFIG_PATH', '')
    env['PKG_CONFIG_PATH'] = os.pathsep.join(
        [p for p in _BREW_PKGCONFIG if os.path.isdir(p)] +
        ([existing] if existing else []))

    try:
        out = subprocess.check_output(['pkg-config', '--cflags', 'sdl3'],
                                      text=True, env=env)
    except (OSError, subprocess.CalledProcessError):
        out = ''
    for token in shlex.split(out):
        if token.startswith('-I') and os.path.isdir(os.path.join(token[2:], 'SDL3')):
            return os.path.join(token[2:], 'SDL3')
        if token.startswith('-I') and os.path.isfile(os.path.join(token[2:], 'SDL.h')):
            return token[2:]
    for guess in ('/opt/homebrew/include/SDL3', '/usr/local/include/SDL3', '/usr/include/SDL3'):
        if os.path.isfile(os.path.join(guess, 'SDL.h')):
            return guess
    sys.exit('could not locate the SDL3 headers (install pkg-config, or brew install sdl3)')


def header_symbols(include_dir):
    '''-> {symbol: header_basename} for every plausible integer constant.'''
    found = {}
    for path in sorted(glob.glob(os.path.join(include_dir, '*.h'))):
        header = os.path.basename(path)
        if header in _SKIP_HEADERS or header.startswith('SDL_config'):
            continue
        with open(path, encoding='utf-8', errors='replace') as handle:
            text = handle.read()

        in_enum = False
        for line in text.splitlines():
            stripped = line.strip()

            match = _DEFINE_RE.match(line)
            if match:
                name, value = match.group(1), match.group(2)
                value = value.split('/*', 1)[0].split('//', 1)[0].strip()
                if name + '(' in line or '"' in value:
                    continue  # function-like macro or string macro (e.g. a hint name)
                looks_numeric = _NUMERIC_BODY_RE.match(value) and any(c.isdigit() for c in value)
                if not (looks_numeric or _REF_BODY_RE.search(value)):
                    continue  # not an integer expression
                found.setdefault(name, header)
                continue

            if stripped.startswith('typedef enum') or stripped == 'enum {' or stripped.startswith('enum SDL'):
                in_enum = True
            if in_enum:
                for name in _ENUM_MEMBER_RE.findall(line):
                    found.setdefault(name, header)
                if '}' in line:
                    in_enum = False
    return found


def registered_symbols():
    with open(_CONSTANTS_C, encoding='utf-8') as handle:
        return set(_REGISTERED_RE.findall(handle.read()))


def module_name(symbol):
    '''Apply the same rename _constants.c uses.'''
    if symbol.startswith('SDLK_'):
        return 'K_' + symbol[5:]
    if symbol.startswith('SDL_'):
        return symbol[4:]
    return symbol  # KMOD_*, AUDIO_*


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--emit', metavar='SUBSTR',
                        help='print PyModule_AddIntConstant lines for missing symbols containing SUBSTR')
    args = parser.parse_args()

    include_dir = sdl_include_dir()
    missing = {sym: hdr for sym, hdr in header_symbols(include_dir).items()
               if sym not in registered_symbols()}

    if args.emit:
        needle = args.emit.upper()
        picked = sorted(sym for sym in missing if needle in sym)
        if not picked:
            sys.exit(f'no missing symbols contain {needle!r}')
        width = max(len(module_name(sym)) for sym in picked)
        for sym in picked:
            print(f'    PyModule_AddIntConstant( module, "{module_name(sym)}",{"":<{width - len(module_name(sym))}} {sym} );')
        return

    by_header = {}
    for sym, hdr in sorted(missing.items()):
        by_header.setdefault(hdr, []).append(sym)
    for hdr in sorted(by_header):
        print(f'# {hdr}  ({len(by_header[hdr])} missing)')
        for sym in by_header[hdr]:
            print(f'    {sym}')
        print()
    print(f'total: {len(missing)} symbols not exposed', file=sys.stderr)


if __name__ == '__main__':
    main()
