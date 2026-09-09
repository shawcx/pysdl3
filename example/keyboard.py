#!/usr/bin/env python3

# Keyboard / text-input / mouse event echo. Type to see TEXTINPUT, hold keys for
# repeat, move the mouse over the window, click to cycle the system cursor.
# Esc or close to quit.

import SDL3

SDL3.Init(SDL3.INIT_VIDEO)

window = SDL3.Window('keyboard', (480, 240))
renderer = window.CreateRenderer()
renderer.SetRenderDrawColor(20, 20, 28, 255)
renderer.Clear()
renderer.Present()

SDL3.StartTextInput(window)
SDL3.SetTextInputArea(window, (8, 8, 200, 24))

CURSORS = [SDL3.SYSTEM_CURSOR_DEFAULT, SDL3.SYSTEM_CURSOR_POINTER,
           SDL3.SYSTEM_CURSOR_CROSSHAIR, SDL3.SYSTEM_CURSOR_WAIT]
cursors = [SDL3.Cursor(c) for c in CURSORS]
which = 0
cursors[which].Set()

typed = ''
running = True
while running:
    event = SDL3.WaitEvent()
    if event is None:
        continue
    kind, data = event

    if kind == SDL3.EVENT_QUIT:
        running = False

    elif kind == SDL3.EVENT_KEY_DOWN:
        state, scancode, sym, mod, repeat = data
        name = SDL3.GetKeyName(sym) or SDL3.GetScancodeName(scancode)
        print(f'KEYDOWN  {name!r:<14} sym={sym} scancode={scancode} '
              f'mod={SDL3.GetModState():#06x} repeat={bool(repeat)}')
        if sym == SDL3.K_ESCAPE:
            running = False
        elif sym == SDL3.K_BACKSPACE:
            typed = typed[:-1]
            print('  buffer:', repr(typed))

    elif kind == SDL3.EVENT_TEXT_INPUT:
        typed += data
        print(f'TEXTINPUT {data!r}   buffer: {typed!r}')

    elif kind == SDL3.EVENT_TEXT_EDITING:
        text, start, length = data
        print(f'TEXTEDITING text={text!r} start={start} length={length}')

    elif kind == SDL3.EVENT_MOUSE_MOTION:
        _state, x, y, xrel, yrel = data
        print(f'MOUSEMOTION ({x:>4},{y:>4}) rel ({xrel:+d},{yrel:+d})  '
              f'focus={SDL3.GetMouseFocus() is not None}')

    elif kind == SDL3.EVENT_MOUSE_BUTTON_DOWN:
        which = (which + 1) % len(cursors)
        cursors[which].Set()
        print(f'cursor -> {CURSORS[which]}')

SDL3.StopTextInput(window)
SDL3.Quit()
