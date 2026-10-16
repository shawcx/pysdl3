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

--generate wraps every constant that is missing from the oldest supported SDL
release in #if SDL_VERSION_ATLEAST(...), comparing against the headers of each
release in _GEN_RELEASES (downloaded once into ~/.cache/pysdl3-headers).
'''

import argparse
import glob
import os
import re
import shlex
import subprocess
import sys
import urllib.request

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
    '''Apply the same rename _constants.c uses: SDLK_x -> K_x, otherwise drop the
    SDL_ prefix (SDL_KMOD_* -> KMOD_*, SDL_AUDIO_* -> AUDIO_*, SDL_EVENT_* ->
    EVENT_*).'''
    if symbol.startswith('SDLK_'):
        return 'K_' + symbol[5:]
    if symbol.startswith('SDL_'):
        return symbol[4:]
    return symbol


# --generate: the curated SDL3 constant table. Headers whose integer symbols we
# expose (matching the subsystem coverage the binding had under SDL2), and a
# blocklist of pixel-format construction machinery / string-property names /
# internal helpers that are not runtime constants.
_GEN_HEADERS = [
    'SDL_init.h', 'SDL_events.h', 'SDL_video.h', 'SDL_mouse.h', 'SDL_keyboard.h',
    'SDL_keycode.h', 'SDL_scancode.h', 'SDL_pixels.h', 'SDL_blendmode.h',
    'SDL_render.h', 'SDL_surface.h', 'SDL_joystick.h', 'SDL_gamepad.h',
    'SDL_haptic.h', 'SDL_sensor.h', 'SDL_touch.h', 'SDL_power.h', 'SDL_audio.h',
    'SDL_messagebox.h', 'SDL_hints.h', 'SDL_timer.h',
]

_GEN_BLOCK = re.compile(
    r'_INTERNAL$'
    r'|^SDL_PROP_'
    r'|^SDL_(PIXELTYPE|BITMAPORDER|PACKEDORDER|ARRAYORDER|PACKEDLAYOUT)_'
    r'|^SDL_DEFINE_'
    r'|^SDL_(ISPIXELFORMAT|PIXELFLAG|PIXELORDER|PIXELLAYOUT|PIXELTYPE)'
    r'|^SDL_(BYTESPERPIXEL|BITSPERPIXEL)'
    r'|^SDL_(COLORSPACE|COLOR_PRIMARIES|COLOR_RANGE|COLOR_TYPE|TRANSFER_CHARACTERISTICS|MATRIX_COEFFICIENTS|CHROMA_LOCATION)'
    r'|^SDL_FOURCC'
    r'|^SDL_WINDOWPOS_ISUNDEFINED|^SDL_WINDOWPOS_ISCENTERED'
    r'|^SDLK_SCANCODE_MASK|^SDLK_EXTENDED_MASK'
    # removed / reserved-but-undefined in SDL3
    r'|^SDL_WINDOWEVENT|^SDL_SYSWMEVENT$|^SDL_DISPLAYEVENT$'
    r'|^SDL_SCANCODE_LOCKING'
    r'|^SDL_STANDARD_GRAVITY$'  # float, not an int constant
    r'|_ENUM_PADDING$|^SDL_HAPTIC_RESERVED|^SDL_MESSAGEBOX_COLOR_COUNT$'
    r'|_FLOAT$'  # SDL_ALPHA_*_FLOAT etc. are float literals, not int constants
)

# Synthesized constants with no single SDL3 symbol, appended after the generated
# block. SDL3 dropped SDL_INIT_EVERYTHING; the binding keeps it as the set the
# no-argument Init() starts.
_GEN_EXTRA = [
    ('INIT_EVERYTHING',
     'SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD'),
    ('INIT_GAMECONTROLLER', 'SDL_INIT_GAMEPAD'),  # SDL2 name kept as an alias
    # Audio.GetStatus() values (SDL3 dropped the SDL_AudioStatus enum).
    ('AUDIO_STOPPED', '0'),
    ('AUDIO_PLAYING', '1'),
    ('AUDIO_PAUSED',  '2'),
    # sentinel touch/mouse ids (the (type)-1 #defines the scanner skips)
    ('TOUCH_MOUSEID', '(long)SDL_TOUCH_MOUSEID'),
    ('MOUSE_TOUCHID', '(long)SDL_MOUSE_TOUCHID'),
]


# SDL releases the binding builds against, oldest first. A generated constant
# missing from the oldest is guarded with the first listed release that has it,
# or with the installed version when none does (a point-release addition).
_GEN_RELEASES = [(3, 2, 0), (3, 4, 0)]
_HEADER_CACHE = os.path.expanduser('~/.cache/pysdl3-headers')
_HEADER_URL = 'https://raw.githubusercontent.com/libsdl-org/SDL/release-{}.{}.{}/include/SDL3/{}'


def release_headers(version):
    '''-> include dir holding the _GEN_HEADERS of an SDL release, downloading
    any that are not cached yet.'''
    root = os.path.join(_HEADER_CACHE, '{}.{}.{}'.format(*version))
    os.makedirs(root, exist_ok=True)
    for hdr in _GEN_HEADERS:
        path = os.path.join(root, hdr)
        if not os.path.isfile(path):
            with urllib.request.urlopen(_HEADER_URL.format(*version, hdr)) as resp:
                data = resp.read()
            with open(path, 'wb') as handle:
                handle.write(data)
    return root


def installed_version(include_dir):
    with open(os.path.join(include_dir, 'SDL_version.h'), encoding='utf-8') as handle:
        text = handle.read()
    return tuple(int(re.search(rf'#define\s+SDL_{part}_VERSION\s+(\d+)', text).group(1))
                 for part in ('MAJOR', 'MINOR', 'MICRO'))


def generate(include_dir):
    by_header = {}
    for sym, hdr in header_symbols(include_dir).items():
        if hdr not in _GEN_HEADERS or _GEN_BLOCK.search(sym):
            continue
        by_header.setdefault(hdr, set()).add(sym)

    since = {}
    for version in _GEN_RELEASES:
        for sym in header_symbols(release_headers(version)):
            since.setdefault(sym, version)
    installed = installed_version(include_dir)

    out = ['', '#include "pysdl.h"', '',
           '// Generated by tools/gen_constants.py --generate against the SDL3 headers.',
           '// Edit that script (header list / blocklist) and regenerate, or add',
           '// one-off constants by hand below the generated block.', '',
           'void _constants(PyObject *module) {']
    for hdr in _GEN_HEADERS:
        syms = by_header.get(hdr)
        if not syms:
            continue
        picked = sorted(syms, key=lambda s: (not s.startswith('SDLK_'), s))
        width = max(len(module_name(s)) for s in picked)
        out.append(f'\n// {hdr}')
        guard = None
        for s in picked:
            version = since.get(s, installed)
            want = version if version > _GEN_RELEASES[0] else None
            if want != guard:
                if guard:
                    out.append('#endif')
                if want:
                    out.append('#if SDL_VERSION_ATLEAST({}, {}, {})'.format(*want))
                guard = want
            name = module_name(s)
            out.append(f'    PyModule_AddIntConstant( module, "{name}",{" " * (width - len(name))} {s} );')
        if guard:
            out.append('#endif')

    out.append('\n// synthesized (no single SDL3 symbol)')
    for name, expr in _GEN_EXTRA:
        out.append(f'    PyModule_AddIntConstant( module, "{name}", {expr} );')
    out.append('}')
    return '\n'.join(out) + '\n'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--emit', metavar='SUBSTR',
                        help='print PyModule_AddIntConstant lines for missing symbols containing SUBSTR')
    parser.add_argument('--generate', action='store_true',
                        help='print a complete src/_constants.c for the current SDL3 headers')
    args = parser.parse_args()

    include_dir = sdl_include_dir()

    if args.generate:
        sys.stdout.write(generate(include_dir))
        return

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
