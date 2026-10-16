#!/usr/bin/env python3

# Dumps every event with a readable name. A custom event is registered and
# pushed once a second from a background thread to show cross-thread PushEvent
# and RegisterEvents. An event filter throws away mouse-motion spam, and an
# event watcher counts what gets through, on whichever thread pushed it. A
# hint callback reports changes to SDL_MOUSE_DOUBLE_CLICK_TIME (press H to
# cycle it). Esc or close to quit.
#
#   --seconds N quits after N seconds and cycles the hint on every tick (with
#   SDL_VIDEO_DRIVER=dummy it runs headless).

import collections
import sys
import threading
import SDL3

SECONDS = float(sys.argv[sys.argv.index('--seconds') + 1]) if '--seconds' in sys.argv else None

SDL3.Init(SDL3.INIT_VIDEO | SDL3.INIT_EVENTS)

TICK = SDL3.RegisterEvents(1)

# name lookup for the type ids we might see
NAMES = {getattr(SDL3, n): n for n in dir(SDL3)
         if n.startswith('EVENT_') and not n.endswith(('_FIRST', '_LAST'))}
NAMES[TICK] = 'TICK (custom)'


def drop_mouse_motion(event):
    return event[0] != SDL3.EVENT_MOUSE_MOTION


SDL3.SetEventFilter(drop_mouse_motion)

# Watchers see every event the filter keeps, as it is pushed - here also on the
# ticker thread - without consuming it.
counts = collections.Counter()
counts_lock = threading.Lock()


def count_events(event):
    with counts_lock:
        counts[event[0]] += 1


SDL3.AddEventWatch(count_events)

window = SDL3.Window('events', (400, 160))  # after the watcher: it sees the window's events too

HINT = 'SDL_MOUSE_DOUBLE_CLICK_TIME'
CLICK_TIMES = ['200', '500', '1000']


def hint_changed(name, old, new):
    print(f'  hint {name}: {old!r} -> {new!r}')


SDL3.AddHintCallback(HINT, hint_changed)  # fires once now with the current value


def cycle_hint():
    CLICK_TIMES.append(CLICK_TIMES.pop(0))
    SDL3.SetHint(HINT, CLICK_TIMES[0])


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
    event = SDL3.WaitEventTimeout(100) if SECONDS else SDL3.WaitEvent()
    if event is None:
        continue
    kind, data = event
    print(f'{NAMES.get(kind, kind):>28}  {data}')

    if kind == SDL3.EVENT_QUIT:
        running = False
    elif kind == SDL3.EVENT_KEY_DOWN and data[2] == SDL3.K_ESCAPE:
        running = False
    elif kind == SDL3.EVENT_KEY_DOWN and data[2] == SDL3.K_H:
        cycle_hint()
    elif kind == TICK and SECONDS:
        cycle_hint()
        if data[0] >= SECONDS:
            running = False

stop.set()
SDL3.RemoveHintCallback(HINT, hint_changed)
SDL3.RemoveEventWatch(count_events)
SDL3.SetEventFilter(None)
with counts_lock:
    print('events seen by the watcher:',
          ', '.join(f'{NAMES.get(k, k)} x{n}' for k, n in counts.most_common()))
SDL3.Quit()
