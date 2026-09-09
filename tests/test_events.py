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
