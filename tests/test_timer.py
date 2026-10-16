'''SDL3.Timer (callback fires on SDL's timer thread).'''

import threading
import time

import pytest


def _wait(condition, timeout=3.0):
    '''Poll instead of sleeping a fixed time: CI machines (macOS especially,
    which coalesces timers) can run a 10 ms timer far slower than asked.'''
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if condition():
            return True
        time.sleep(0.01)
    return condition()


def test_timer_fires_and_stops_on_zero(sdl):
    hits = []

    def callback(interval):
        hits.append(interval)
        return 0 if len(hits) >= 3 else interval

    timer = sdl.Timer(20, callback)
    assert timer.id != 0
    assert _wait(lambda: len(hits) >= 3)
    time.sleep(0.1)  # it returned 0 on the third call: no fourth may follow

    assert len(hits) == 3
    assert hits[0] == 20
    assert timer.id != 0  # object still valid, timer just self-stopped


def test_timer_none_return_repeats_until_removed(sdl):
    count = [0]

    def callback(interval):
        count[0] += 1
        return None

    timer = sdl.Timer(10, callback)
    assert _wait(lambda: count[0] > 3)  # None keeps it repeating

    timer.Remove()
    fired = count[0]
    assert timer.id == 0
    time.sleep(0.05)
    assert count[0] == fired  # no more callbacks after Remove


def test_timer_remove_is_idempotent(sdl):
    timer = sdl.Timer(1000, lambda i: 0)
    timer.Remove()
    timer.Remove()
    assert timer.id == 0


def test_timer_requires_callable(sdl):
    with pytest.raises(TypeError):
        sdl.Timer(10, 123)


def test_timer_callback_runs_on_another_thread(sdl):
    main = threading.get_ident()
    thread_ids = []

    def callback(interval):
        thread_ids.append(threading.get_ident())
        return 0

    timer = sdl.Timer(10, callback)
    assert _wait(lambda: thread_ids)
    assert timer.id is not None  # keep the timer alive until it fires
    assert thread_ids[0] != main


def test_empty_timer_has_no_id(sdl):
    assert sdl.Timer().id == 0


def test_dropping_timer_object_removes_it(sdl):
    import gc

    count = [0]
    timer = sdl.Timer(10, lambda i: count.__setitem__(0, count[0] + 1) or None)
    assert _wait(lambda: count[0] > 0)

    del timer
    gc.collect()
    fired = count[0]
    time.sleep(0.05)
    assert count[0] == fired  # the timer stopped when its wrapper was collected


def test_dropping_timers_mid_callback_is_safe(sdl):
    # SDL_RemoveTimer doesn't wait for a callback already under way. Timers used
    # to hand SDL the Python callable itself, so dropping one while its callback
    # waited for the GIL called a freed object (a segfault in every run of this
    # loop). Run in a subprocess: the failure mode is a crash.
    import os
    import subprocess
    import sys
    code = (
        'import gc, SDL3\n'
        'SDL3.Init(SDL3.INIT_EVENTS)\n'
        'for _ in range(1500):\n'
        '    hits = []\n'
        '    timers = [SDL3.Timer(1, lambda i, h=hits: h.append(i) or None) for _ in range(4)]\n'
        '    for _ in range(2000):\n'
        '        pass\n'
        '    del timers\n'
        '    gc.collect()\n'
        'print("ok")\n'
    )
    env = dict(os.environ, PYTHONPATH=os.path.dirname(sdl.__file__))
    out = subprocess.run([sys.executable, '-c', code], env=env, capture_output=True,
                         text=True, timeout=60)
    assert out.returncode == 0 and out.stdout.strip() == 'ok', (out.returncode, out.stderr)


def test_timer_can_remove_itself(sdl):
    fired = []
    holder = {}

    def once(interval):
        fired.append(interval)
        holder['timer'].Remove()  # from inside its own callback
        return interval

    holder['timer'] = sdl.Timer(5, once)
    assert _wait(lambda: fired)
    time.sleep(0.05)  # removed itself: no second call may follow
    assert len(fired) == 1
    assert holder['timer'].id == 0


def _run_script(sdl, code, timeout=20):
    '''Run `code` in a fresh interpreter, killed after `timeout` s so a
    deadlock fails the test instead of hanging the suite; it must print "ok".'''
    import os
    import subprocess
    import sys
    import textwrap
    env = dict(os.environ, PYTHONPATH=os.path.dirname(sdl.__file__))
    try:
        out = subprocess.run([sys.executable, '-c', textwrap.dedent(code)], env=env,
                             capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        pytest.fail('deadlocked (timed out)')
    assert out.returncode == 0 and out.stdout.strip().endswith('ok'), out.stderr or out.stdout


def test_quit_while_a_timer_callback_runs(sdl):
    '''Regression (phase 13): SDL_Quit joins the timer thread, which may be in
    a Python callback waiting for the GIL - Remove() doesn't wait for a call
    already under way - so Quit holding the GIL deadlocked within a few of
    these cycles. Init / InitSubSystem / QuitSubSystem / Quit drop the GIL.'''
    _run_script(sdl, '''
        import time, SDL3

        def tick(interval):
            sum(range(3000))  # long enough to be preempted
            return interval

        for _ in range(100):
            SDL3.Init(SDL3.INIT_EVENTS)
            timers = [SDL3.Timer(1, tick) for _ in range(3)]
            deadline = time.monotonic() + 0.005
            while time.monotonic() < deadline:
                sum(range(100))
            timers[0].Remove()
            SDL3.Quit()
            del timers
        print('ok')
    ''')


def test_quit_while_a_timer_callback_pushes_events(sdl):
    '''Regression (phase 13): SDL_Quit tears the event subsystem down before
    it stops the timer thread, and Remove() doesn't wait for a callback under
    way, so a callback inside PushEvent could block forever on the watcher
    lock SDL_Quit had just destroyed (hung on the first run on Linux). Quit
    now holds Timer callbacks off and waits for one already running.'''
    _run_script(sdl, '''
        import time, SDL3

        def busy(event):
            sum(range(300))
            return True

        for _ in range(100):
            SDL3.Init(SDL3.INIT_EVENTS)
            T = SDL3.RegisterEvents(1)
            SDL3.AddEventWatch(busy)
            SDL3.SetEventFilter(busy)
            timer = SDL3.Timer(1, lambda interval: (SDL3.PushEvent(T, 1), interval)[1])
            deadline = time.monotonic() + 0.003
            while time.monotonic() < deadline:
                SDL3.PushEvent(T, 2)
            timer.Remove()               # doesn't wait for a callback under way
            SDL3.SetEventFilter(None)    # contend with it for the watcher lock
            SDL3.RemoveEventWatch(busy)
            SDL3.Quit()
        print('ok')
    ''')


def test_quit_audio_while_its_callback_runs(sdl):
    '''Same cycle through QuitSubSystem(INIT_AUDIO): SDL_QuitAudio closes the
    device while its thread holds it inside the Python playback callback.
    The Audio objects are never freed (os._exit): one kept past the audio
    subsystem still crashes in its dealloc, a separate, older bug.'''
    _run_script(sdl, '''
        import os, time, SDL3
        keep = []

        def callback(size, userdata):
            sum(range(3000))
            return b'\\x00' * size

        for _ in range(30):
            SDL3.InitSubSystem(SDL3.INIT_AUDIO)
            audio = SDL3.Audio()
            audio.Open(freq=48000, channels=1, samples=256, callback=callback)
            audio.Pause(0)
            deadline = time.monotonic() + 0.02
            while time.monotonic() < deadline:
                sum(range(100))
            SDL3.QuitSubSystem(SDL3.INIT_AUDIO)
            keep.append(audio)
        print('ok', flush=True)
        os._exit(0)
    ''')
