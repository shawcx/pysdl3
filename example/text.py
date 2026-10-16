#!/usr/bin/env python3

# SDL3_ttf text rendering: the four render modes, styles, a wrapped paragraph
# that re-flows as the window is resized, and live font metrics.
#
#   python3 example/text.py [--font PATH] [--frames N]
#
# Needs the extension built with SDL3_ttf (libsdl3-ttf-dev / brew sdl3_ttf).
# Esc or close to quit; + / - change the paragraph's point size.

import glob
import shutil
import subprocess
import sys
import SDL3

if not hasattr(SDL3, 'Font'):
    sys.exit('this SDL3 build has no SDL3_ttf support')

CANDIDATES = [
    '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
    '/System/Library/Fonts/Supplemental/Arial.ttf',
    'C:/Windows/Fonts/arial.ttf',
]


def arg(name, default=None):
    return sys.argv[sys.argv.index(name) + 1] if name in sys.argv else default


def find_font():
    for path in CANDIDATES:
        if glob.glob(path):
            return path
    if shutil.which('fc-match'):
        path = subprocess.run(['fc-match', '-f', '%{file}', 'sans:fontformat=TrueType'],
                              capture_output=True, text=True).stdout.strip()
        if path:
            return path
    sys.exit('no font found: pass --font PATH')


font_path = arg('--font') or find_font()
frames_left = int(arg('--frames', 0)) or None

SDL3.Init(SDL3.INIT_VIDEO)
SDL3.TTF_Init()

window = SDL3.Window('text', (640, 400), flags=SDL3.WINDOW_RESIZABLE)
renderer = window.CreateRenderer()
renderer.SetVSync(1)

title = SDL3.Font(font_path, 28)
body = SDL3.Font(font_path, 16)
print(f'{title.FaceFamilyName()} {title.FaceStyleName()}  ({font_path})')
print(f'height {title.Height()}  ascent {title.Ascent()}  descent {title.Descent()}  '
      f'line skip {title.LineSkip()}')
print(f'SDL_ttf {SDL3.TTF_Version()}  FreeType {SDL3.TTF_GetFreeTypeVersion()}  '
      f'HarfBuzz {SDL3.TTF_GetHarfBuzzVersion()}')

PARAGRAPH = ('SDL_ttf renders UTF-8 text with FreeType. Wrapped text breaks at '
             'word boundaries to fit a pixel width, so this paragraph re-flows '
             'when you resize the window. Ünïcödé wörks tøø: αβγ ✓')


def texture(surface):
    return renderer.CreateTextureFromSurface(surface)


def draw(surface_or_texture, x, y):
    tex = surface_or_texture if hasattr(surface_or_texture, 'Query') else texture(surface_or_texture)
    w, h = tex.Query()[2:]
    renderer.Copy(tex, None, (x, y, w, h))
    return h


def mode_row():
    # One sample per render mode, drawn side by side.
    samples = [
        title.RenderSolid('Solid', (255, 220, 120)),
        title.RenderShaded('Shaded', (255, 255, 255), (60, 60, 120)),
        title.RenderBlended('Blended', (120, 220, 255)),
        title.RenderLCD('LCD', (255, 255, 255), (32, 32, 40)),
    ]
    return [texture(s) for s in samples]


def style_row():
    out = []
    for name in ('BOLD', 'ITALIC', 'UNDERLINE', 'STRIKETHROUGH'):
        body.SetStyle(getattr(SDL3, 'TTF_STYLE_' + name))
        out.append(texture(body.RenderBlended(name.lower(), (230, 230, 230))))
    body.SetStyle(SDL3.TTF_STYLE_NORMAL)
    return out


modes = mode_row()
styles = style_row()
para_size = 16
running = True
while running:
    while (event := SDL3.PollEvent()) is not None:
        kind, data = event
        if kind == SDL3.EVENT_QUIT or (kind == SDL3.EVENT_KEY_DOWN and data[2] == SDL3.K_ESCAPE):
            running = False
        elif kind == SDL3.EVENT_KEY_DOWN and data[2] in (SDL3.K_PLUS, SDL3.K_EQUALS, SDL3.K_MINUS):
            para_size = max(8, para_size + (-2 if data[2] == SDL3.K_MINUS else 2))
            body.SetSize(para_size)
            styles = style_row()

    width, _ = renderer.GetRendererOutputSize()
    renderer.SetRenderDrawColor(32, 32, 40, 255)
    renderer.Clear()

    x, y = 16, 16
    for tex in modes:
        draw(tex, x, y)
        x += tex.Query()[2] + 16
    y += title.LineSkip() + 12

    x = 16
    for tex in styles:
        draw(tex, x, y)
        x += tex.Query()[2] + 16
    y += body.LineSkip() + 16

    # Re-rendered each frame so it follows the window width.
    y += draw(body.RenderBlended(PARAGRAPH, (220, 220, 220), wrap_length=width - 32), 16, y)

    fitted, count = body.Measure(PARAGRAPH, width - 32)
    status = f'{count} chars fit on one {width - 32}px line ({fitted}px) - +/- resizes'
    draw(body.RenderBlended(status, (140, 140, 160)), 16, y + 12)

    renderer.Present()
    if frames_left is not None:
        frames_left -= 1
        running = running and frames_left > 0

for font in (title, body):
    font.Close()  # fonts must be closed before TTF_Quit
SDL3.TTF_Quit()
SDL3.Quit()
