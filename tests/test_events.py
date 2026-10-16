'''Phase 5: the event queue - push/poll/peep/flush, filters, custom events.'''

import time

import pytest


@pytest.fixture
def drained(sdl):
    '''A quiet event queue and a freshly registered custom event type.'''
    sdl.PumpEvents()
    while sdl.PollEvent() is not None:
        pass
    return sdl.RegisterEvents(2)


def test_push_poll_roundtrip(sdl, drained):
    mytype = drained
    assert sdl.PushEvent(mytype, 42, 7) is True
    assert sdl.PollEvent() == (mytype, (42, 7))
    assert sdl.PollEvent() is None


def test_has_event_and_flush(sdl, drained):
    a, b = drained, drained + 1
    sdl.PushEvent(a)
    sdl.PushEvent(b)
    assert sdl.HasEvent(a) is True
    assert sdl.HasEvents(a, b) is True

    sdl.FlushEvent(a)
    assert sdl.HasEvent(a) is False
    assert sdl.HasEvent(b) is True

    sdl.FlushEvents(a, b)
    assert sdl.HasEvents(a, b) is False


def test_peep_events_peek_then_get(sdl, drained):
    mytype = drained
    for code in (10, 20, 30):
        sdl.PushEvent(mytype, code)

    peeked = sdl.PeepEvents(10, sdl.PEEKEVENT, mytype, mytype)
    assert [data[0] for _, data in peeked] == [10, 20, 30]
    assert sdl.HasEvent(mytype) is True  # peek does not remove

    got = sdl.PeepEvents(10, sdl.GETEVENT, mytype, mytype)
    assert [data[0] for _, data in got] == [10, 20, 30]
    assert sdl.HasEvent(mytype) is False

    with pytest.raises(ValueError):
        sdl.PeepEvents(1, sdl.ADDEVENT)


def test_event_state(sdl):
    previous = sdl.EventState(sdl.EVENT_MOUSE_MOTION, 0)
    try:
        assert sdl.GetEventState(sdl.EVENT_MOUSE_MOTION) is False
    finally:
        sdl.EventState(sdl.EVENT_MOUSE_MOTION, previous)


def test_wait_event_timeout_returns_none_when_idle(sdl, drained):
    start = time.monotonic()
    assert sdl.WaitEventTimeout(30) is None
    assert (time.monotonic() - start) * 1000 >= 15


def test_wait_event_timeout_delivers_pushed_event(sdl, drained):
    sdl.PushEvent(drained, 99)
    assert sdl.WaitEventTimeout(100) == (drained, (99, 0))


def test_quit_requested(sdl, drained):
    assert sdl.QuitRequested() is False
    sdl.PushEvent(sdl.EVENT_QUIT)
    assert sdl.QuitRequested() is True
    sdl.FlushEvent(sdl.EVENT_QUIT)


def test_event_filter_drops_and_keeps(sdl, drained):
    a, b = drained, drained + 1
    seen = []

    def keep_only_a(event):
        seen.append(event[0])
        return event[0] == a

    sdl.SetEventFilter(keep_only_a)
    assert sdl.GetEventFilter() is keep_only_a
    try:
        sdl.PushEvent(a, 1)
        sdl.PushEvent(b, 2)
        delivered = []
        while (event := sdl.PollEvent()) is not None:
            delivered.append(event[0])
    finally:
        sdl.SetEventFilter(None)

    assert set(seen) == {a, b}
    assert delivered == [a]
    assert sdl.GetEventFilter() is None


def test_filter_events_over_queue(sdl, drained):
    mytype = drained
    for code in (1, 2, 3):
        sdl.PushEvent(mytype, code)
    sdl.FilterEvents(lambda event: event[1][0] != 2)
    remaining = [data[0] for _, data in sdl.PeepEvents(10, sdl.GETEVENT, mytype, mytype)]
    assert remaining == [1, 3]


def test_set_event_filter_rejects_non_callable(sdl):
    with pytest.raises(TypeError):
        sdl.SetEventFilter(42)


# --- phase 13: event watchers, the GIL rule ----------------------------------

def test_event_watch_sees_pushed_events(sdl, drained):
    seen = []

    def watch(event):
        if event[0] == drained:  # PollEvent also pumps SDL's own events
            seen.append(event)

    sdl.AddEventWatch(watch)
    try:
        sdl.PushEvent(drained, 7)
        assert seen == [(drained, (7, 0))]
        assert sdl.PollEvent() == (drained, (7, 0))  # still queued; watching is passive
    finally:
        sdl.RemoveEventWatch(watch)
    sdl.PushEvent(drained, 8)
    assert len(seen) == 1
    sdl.FlushEvent(drained)


def test_event_watch_runs_after_filter(sdl, drained):
    seen = []

    def watch(event):
        if event[0] == drained:
            seen.append(event[1][0])

    sdl.AddEventWatch(watch)
    sdl.SetEventFilter(lambda event: event[0] != drained or event[1][0] != 2)
    try:
        assert sdl.PushEvent(drained, 1) is True
        assert sdl.PushEvent(drained, 2) is False  # dropped by the filter: watchers never see it
    finally:
        sdl.SetEventFilter(None)
        sdl.DelEventWatch(watch)  # pysdl2's name
    assert seen == [1]
    sdl.FlushEvent(drained)


def test_event_watch_registration_errors(sdl):
    with pytest.raises(TypeError):
        sdl.AddEventWatch(42)
    with pytest.raises(ValueError):
        sdl.RemoveEventWatch(lambda event: None)


def test_event_watch_registered_twice(sdl, drained):
    calls = []

    def watch(event):
        if event[0] == drained:
            calls.append(event[1][0])

    sdl.AddEventWatch(watch)
    sdl.AddEventWatch(watch)
    sdl.PushEvent(drained, 1)
    assert calls == [1, 1]
    sdl.RemoveEventWatch(watch)  # drops one registration
    sdl.PushEvent(drained, 2)
    assert calls == [1, 1, 2]
    sdl.RemoveEventWatch(watch)
    sdl.PushEvent(drained, 3)
    assert calls == [1, 1, 2]
    with pytest.raises(ValueError):
        sdl.RemoveEventWatch(watch)
    sdl.FlushEvent(drained)


def test_event_watch_can_remove_itself_and_others(sdl, drained):
    calls = []

    def first(event):
        if event[0] == drained:
            calls.append(('first', event[1][0]))
            sdl.RemoveEventWatch(first)
            sdl.RemoveEventWatch(second)  # later in this same dispatch: skipped

    def second(event):
        if event[0] == drained:
            calls.append(('second', event[1][0]))

    sdl.AddEventWatch(first)
    sdl.AddEventWatch(second)
    sdl.PushEvent(drained, 1)
    sdl.PushEvent(drained, 2)
    assert calls == [('first', 1)]
    sdl.FlushEvent(drained)


def test_event_watch_releases_callable(sdl):
    import gc
    import weakref

    class Watch:
        def __call__(self, event):
            pass

    watch = Watch()
    ref = weakref.ref(watch)
    sdl.AddEventWatch(watch)
    del watch
    gc.collect()
    assert ref() is not None  # the registration keeps it alive
    sdl.RemoveEventWatch(ref())
    gc.collect()
    assert ref() is None


def test_event_watch_exception_is_printed(sdl, drained, capsys):
    def broken(event):
        raise RuntimeError('watcher failed')

    sdl.AddEventWatch(broken)
    try:
        assert sdl.PushEvent(drained, 1) is True  # still queued
    finally:
        sdl.RemoveEventWatch(broken)
    assert 'watcher failed' in capsys.readouterr().err
    assert sdl.PollEvent() == (drained, (1, 0))


def test_event_watch_during_exception_unwinding(sdl):
    # Destroying a window pushes EVENT_WINDOW_DESTROYED. Here that happens in
    # the window's dealloc while a KeyError unwinds the frame, so the watcher
    # runs with an exception pending: it must be set aside, not leak into the
    # call (SystemError) or be lost.
    seen = []

    def watch(event):
        seen.append(event[0])

    def make_and_fail():
        window = sdl.Window('unwind', (8, 8))  # noqa: F841 - freed while unwinding
        raise KeyError('boom')

    sdl.AddEventWatch(watch)
    try:
        with pytest.raises(KeyError):
            make_and_fail()
    finally:
        sdl.RemoveEventWatch(watch)
    assert sdl.EVENT_WINDOW_DESTROYED in seen
    sdl.FlushEvents()


def _run_script(sdl, body, timeout=20):
    '''Run `body` in a fresh interpreter (killed after `timeout` s: a deadlock
    fails the test instead of hanging the suite); it must print "ok".'''
    import os
    import subprocess
    import sys
    import textwrap
    code = 'import gc, threading, time, SDL3\n' + textwrap.dedent(body) + '\nprint("ok")\n'
    env = dict(os.environ, PYTHONPATH=os.path.dirname(sdl.__file__))
    try:
        out = subprocess.run([sys.executable, '-c', code], env=env, capture_output=True,
                             text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        pytest.fail('deadlocked (timed out)')
    assert out.returncode == 0 and out.stdout.strip().endswith('ok'), out.stderr or out.stdout


def test_event_callbacks_cross_thread_no_deadlock(sdl):
    # The filter and watchers run under SDL's watcher lock and then take the
    # GIL, which their Python code may hand to another thread mid-call. Before
    # the GIL rule a second pushing thread then held the GIL while waiting for
    # that lock: a deadlock within milliseconds. Pushes come from an SDL timer
    # thread, a Python thread and the main thread (which also polls, flushes
    # and swaps the filter - the swap used to be able to free a filter another
    # thread was about to call).
    _run_script(sdl, '''
        SDL3.Init(SDL3.INIT_VIDEO | SDL3.INIT_EVENTS)
        TICK = SDL3.RegisterEvents(1)
        threads = set()

        def busy(event):
            sum(range(500))  # long enough to be preempted
            return True

        def watch(event):
            if event[0] == TICK:
                threads.add(threading.get_ident())
            busy(event)

        def tick(interval):
            SDL3.PushEvent(TICK, 1)
            return interval

        SDL3.AddEventWatch(watch)
        SDL3.SetEventFilter(busy)
        timer = SDL3.Timer(1, tick)
        stop = time.monotonic() + 1.0

        def pusher():
            while time.monotonic() < stop:
                SDL3.PushEvent(TICK, 2)

        thread = threading.Thread(target=pusher)
        thread.start()
        n = 0
        while time.monotonic() < stop:
            SDL3.PushEvent(TICK, 3)
            SDL3.PollEvent()
            SDL3.HasEvent(TICK)
            n += 1
            if n % 50 == 0:
                SDL3.SetEventFilter(lambda event: busy(event))
            if n % 200 == 0:
                SDL3.FlushEvent(TICK)
        thread.join()
        timer.Remove()
        SDL3.SetEventFilter(None)
        SDL3.RemoveEventWatch(watch)
        assert len(threads) >= 2, threads  # the watcher ran on other threads too
        SDL3.Quit()
    ''')


def test_quit_drops_event_filter_and_watches(sdl):
    # Stopping the event subsystem makes SDL forget the filter and watchers;
    # the binding forgets them too rather than reporting stale registrations.
    _run_script(sdl, '''
        SDL3.Init(SDL3.INIT_EVENTS)
        calls = []
        watch = lambda event: calls.append(event)
        keep = lambda event: True
        SDL3.SetEventFilter(keep)
        SDL3.AddEventWatch(watch)

        SDL3.InitSubSystem(SDL3.INIT_EVENTS)   # second reference
        SDL3.QuitSubSystem(SDL3.INIT_EVENTS)   # events keep running
        assert SDL3.GetEventFilter() is keep
        SDL3.QuitSubSystem(SDL3.INIT_EVENTS)   # now they stop
        assert SDL3.GetEventFilter() is None
        try:
            SDL3.RemoveEventWatch(watch)
            raise SystemExit('watch still registered')
        except ValueError:
            pass

        SDL3.Init(SDL3.INIT_EVENTS)
        SDL3.AddEventWatch(watch)
        SDL3.Quit()
        assert SDL3.GetEventFilter() is None
        SDL3.Init(SDL3.INIT_EVENTS)
        SDL3.PushEvent(SDL3.RegisterEvents(1))
        assert calls == []
        SDL3.Quit()
    ''')
