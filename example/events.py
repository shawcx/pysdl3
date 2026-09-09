#!/usr/bin/env python3

# Dumps every event with a readable name. A custom event is registered and
# pushed once a second from a background thread to show cross-thread PushEvent
# and RegisterEvents. An event filter throws away mouse-motion spam.
# Esc or close to quit.

import threading
import time
import SDL3

SDL3.Init(SDL3.INIT_VIDEO | SDL3.INIT_EVENTS)

window = SDL3.Window('events', (400, 160))

TICK = SDL3.RegisterEvents(1)

# name lookup for the type ids we might see
NAMES = {getattr(SDL3, n): n for n in dir(SDL3)
         if n.isupper() and isinstance(getattr(SDL3, n), int)
         and (n in ('QUIT',) or n.endswith(('EVENT', 'DOWN', 'UP', 'MOTION', 'WHEEL',
                                            'ADDED', 'REMOVED', 'REMAPPED', 'INPUT',
                                            'EDITING', 'FILE', 'TEXT', 'BEGIN', 'COMPLETE',
                                            'GESTURE', 'RECORD', 'UPDATE', 'CHANGED')))}
NAMES[TICK] = 'TICK (custom)'


def drop_mouse_motion(event):
    return event[0] != SDL3.EVENT_MOUSE_MOTION


SDL3.SetEventFilter(drop_mouse_motion)

stop = threading.Event()


def ticker():
    n = 0
    while not stop.wait(1.0):
        n += 1
        SDL3.PushEvent(TICK, code=n)


threading.Thread(target=ticker, daemon=True).start()

print('waiting for events (mouse-motion is filtered out)...')
running = True
while running:
    event = SDL3.WaitEvent()
    if event is None:
        continue
    kind, data = event
    print(f'{NAMES.get(kind, kind):>22}  {data}')

    if kind == SDL3.EVENT_QUIT:
        running = False
    elif kind == SDL3.EVENT_KEY_DOWN and data[2] == SDL3.K_ESCAPE:
        running = False

stop.set()
SDL3.SetEventFilter(None)
SDL3.Quit()
