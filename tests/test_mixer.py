'''SDL3_mixer (phase 16): Mixer / MixerAudio / MixerTrack / MixerGroup /
MixerDecoder and the MIX_* functions. Offline mixers (device=None) are driven
by Generate() on the test thread, so their output is checked sample by sample;
device mixers run on the dummy audio driver's real audio thread. Skipped when
the extension was built without SDL3_mixer.'''

import array
import gc
import io
import math
import os
import struct
import subprocess
import sys
import threading
import time
import wave

import pytest

RATE = 48000
FRAME = 8  # stereo float32


def _wav(ms=100, freq=440, rate=22050):
    '''A mono 16-bit sine WAV file, as bytes.'''
    n = rate * ms // 1000
    samples = [int(8000 * math.sin(2 * math.pi * freq * i / rate)) for i in range(n)]
    buf = io.BytesIO()
    with wave.open(buf, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(struct.pack('<%dh' % n, *samples))
    return buf.getvalue()


def _stereo(left, right, frames):
    return array.array('f', [left, right] * frames).tobytes()


def _channels(data):
    samples = array.array('f', data)
    return samples[0::2], samples[1::2]


def _peak(samples):
    return max((abs(x) for x in samples), default=0.0)


def _wait(condition, timeout=3.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if condition():
            return True
        time.sleep(0.01)
    return condition()


def _run(sdl, code, timeout=60):
    env = dict(os.environ, PYTHONPATH=os.path.dirname(sdl.__file__))
    out = subprocess.run([sys.executable, '-c', code], env=env, capture_output=True,
                         text=True, timeout=timeout)
    assert out.returncode == 0 and out.stdout.strip().endswith('ok'), out.stderr or out.stdout


@pytest.fixture
def mix(sdl):
    if not hasattr(sdl, 'Mixer'):
        pytest.skip('built without SDL3_mixer')
    sdl.MIX_Init()
    yield sdl
    sdl.MIX_Quit()


@pytest.fixture
def offline(mix):
    mixer = mix.Mixer(spec=(RATE, mix.AUDIO_F32, 2), device=None)
    yield mixer
    mixer.Destroy()


def _generate(mixer, frames):
    data, mixed = mixer.Generate(frames * FRAME)
    assert len(data) == frames * FRAME and 0 <= mixed <= len(data)
    return data


def _const_track(mix, mixer, left=0.5, right=0.25, frames=RATE):
    audio = mix.MIX_LoadRawAudio(_stereo(left, right, frames), (RATE, mix.AUDIO_F32, 2), mixer=mixer)
    track = mixer.CreateTrack()
    track.SetAudio(audio)
    return track


# --- library ----------------------------------------------------------------

def test_version_and_decoders(mix):
    assert mix.MIX_Version()[0] == 3
    decoders = mix.MIX_GetAudioDecoders()
    assert 'WAV' in decoders and 'RAW' in decoders
    assert len(decoders) == mix.MIX_GetNumAudioDecoders()
    assert mix.MIX_GetAudioDecoder(0) == decoders[0]
    assert mix.MIX_GetAudioDecoder(9999) is None
    assert mix.MIX_MSToFrames(RATE, 1000) == RATE
    assert mix.MIX_FramesToMS(RATE, RATE // 2) == 500
    assert mix.MIX_DURATION_UNKNOWN == -1 and mix.MIX_DURATION_INFINITE == -2


def test_needs_mix_init(sdl):
    if not hasattr(sdl, 'Mixer'):
        pytest.skip('built without SDL3_mixer')
    with pytest.raises(sdl.error, match='MIX_Init'):
        sdl.Mixer(spec=(RATE, sdl.AUDIO_F32, 2), device=None)
    with pytest.raises(sdl.error, match='MIX_Init'):
        sdl.MixerAudio(_wav())
    sdl.MIX_Quit()  # unmatched: a no-op


# --- offline mixing ---------------------------------------------------------

def test_offline_mixer_plays_and_stops(offline, mix):
    assert offline.GetFormat() == (RATE, mix.AUDIO_F32, 2)
    assert offline.GetDevice() is None
    audio = mix.MIX_CreateSineWaveAudio(440, 0.5, 100)
    assert audio.GetDuration() == RATE // 10
    track = offline.CreateTrack()
    track.SetAudio(audio)
    assert track.GetAudio() is audio and track.GetMixer() is offline
    stopped = []
    track.SetStoppedCallback(lambda t: stopped.append((t, threading.get_ident())))
    track.Play()
    assert track.Playing()
    data, mixed = offline.Generate(RATE // 5 * FRAME)  # 200 ms: the sine runs out halfway
    assert 0 < mixed < len(data)
    left, right = _channels(data)
    assert 0.3 < _peak(left[:RATE // 20]) <= 0.51
    assert _peak(left[-RATE // 20:]) == 0.0  # silence after it ran out
    assert stopped == [(track, threading.get_ident())]  # Generate runs callbacks right here
    assert not track.Playing()


def test_generate_arguments(offline, mix):
    with pytest.raises(mix.error):
        offline.Generate(3)  # not a whole frame
    with pytest.raises(ValueError):
        offline.Generate(-8)
    assert offline.Generate(0) == (b'', 0)
    device = mix.Mixer()
    try:
        with pytest.raises(mix.error, match='offline'):
            device.Generate(8)
    finally:
        device.Destroy()
    with pytest.raises(ValueError, match='spec'):
        mix.Mixer(device=None)
    with pytest.raises(TypeError):
        mix.Mixer(spec=(RATE, mix.AUDIO_F32), device=None)


def test_raw_audio_passes_through_exactly(offline, mix):
    source = bytearray(_stereo(0.5, 0.25, 4800))
    audio = mix.MIX_LoadRawAudio(source, (RATE, mix.AUDIO_F32, 2), mixer=offline)
    source[:] = bytes(len(source))  # copied: the audio keeps its own data
    assert audio.GetDuration() == 4800 and audio.GetFormat() == (RATE, mix.AUDIO_F32, 2)
    track = offline.CreateTrack()
    track.SetAudio(audio)
    track.Play()
    left, right = _channels(_generate(offline, 1000))
    assert set(left) == {0.5} and set(right) == {0.25}
    with pytest.raises(TypeError):
        mix.MIX_LoadRawAudio(b'\0' * 8, (RATE, mix.AUDIO_F32))


# --- audio ------------------------------------------------------------------

def test_audio_sources(mix, tmp_path, offline):
    wav = _wav(100, rate=22050)
    audio = mix.MixerAudio(wav)
    assert audio.GetFormat() == (22050, mix.AUDIO_S16, 1)
    assert audio.GetDuration() == 2205
    assert audio.GetMetadata()['duration_frames'] == 2205
    assert audio.MSToFrames(1000) == 22050 and audio.FramesToMS(22050) == 1000

    path = tmp_path / 'tone.wav'
    path.write_bytes(wav)
    assert mix.MixerAudio(path, predecode=True).GetDuration() == 2205
    assert mix.MixerAudio(str(path), decoder='WAV').GetDuration() == 2205
    assert mix.MixerAudio(memoryview(wav), offline).GetDuration() == 2205
    assert offline.LoadAudio(wav, predecode=True).GetDuration() == 2205
    if mix.MIX_Version() >= (3, 2, 4):
        assert mix.MixerAudio(wav, ignore_loops=True).GetDuration() == 2205

    source = bytearray(wav)
    copy = mix.MixerAudio(source)
    source[:] = bytes(len(source))  # loading read (and cached) all of it
    track = offline.CreateTrack()
    track.SetAudio(copy)
    track.Play()
    assert _peak(_channels(_generate(offline, 2000))[0]) > 0.1

    with pytest.raises(mix.error):
        mix.MixerAudio(wav, decoder='NOPE')
    with pytest.raises(mix.error):
        mix.MixerAudio(b'this is not audio')
    with pytest.raises(TypeError):
        mix.MixerAudio(123)
    with pytest.raises(TypeError):
        mix.MixerAudio(wav, mixer='nope')


def test_audio_destroy_while_assigned(offline, mix):
    audio = mix.MIX_CreateSineWaveAudio(440, 0.5, 1000)
    track = offline.CreateTrack()
    track.SetAudio(audio)
    audio.Destroy()  # SDL_mixer refcounts it: the track still plays
    with pytest.raises(mix.error, match='MixerAudio'):
        audio.GetDuration()
    with pytest.raises(mix.error, match='MixerAudio'):
        track.SetAudio(audio)
    track.Play()
    assert _peak(_channels(_generate(offline, 1000))[0]) > 0.1
    offline.PlayAudio(mix.MIX_CreateSineWaveAudio(220, 0.25, 10))  # fire and forget
    _generate(offline, 1000)


# --- tracks -----------------------------------------------------------------

def test_track_transport(offline, mix):
    track = _const_track(mix, offline, frames=4800)
    with pytest.raises(TypeError):
        track.Play(1)  # options are keywords
    with pytest.raises(TypeError, match='unknown play option'):
        track.Play(nope=1)
    track.Play(loops=2, fade_in_ms=0, start_frame=0, max_ms=-1, fade_in_start_gain=0.0)
    assert track.GetLoops() == 2
    track.SetLoops(3)
    assert track.GetLoops() == 3
    track.Pause()
    assert track.Paused() and not track.Playing()
    track.Resume()
    assert track.Playing() and not track.Paused()
    track.SetPlaybackPosition(100)
    assert track.GetPlaybackPosition() == 100 and track.GetRemaining() == 4700
    assert track.MSToFrames(1000) == RATE and track.FramesToMS(RATE) == 1000
    track.SetLoops(0)
    track.Stop(fade_out_ms=50)
    assert track.GetFadeFrames() == -RATE // 20 and track.Playing()
    _generate(offline, RATE // 10)
    assert not track.Playing() and track.GetFadeFrames() == 0
    track.Play()
    track.Stop()
    assert not track.Playing()
    track.Play(halt_when_exhausted=False)  # keeps "playing" (silence) once the input runs out
    _generate(offline, 9600)
    assert track.Playing() and track.GetRemaining() == 0
    track.Stop()
    with pytest.raises(mix.error):
        offline.CreateTrack().Play()  # no input


def test_gain_and_frequency(offline, mix):
    track = _const_track(mix, offline)
    track.SetGain(0.5)
    assert track.GetGain() == 0.5
    offline.SetGain(0.5)
    assert offline.GetGain() == 0.5
    track.Play()
    left, _ = _channels(_generate(offline, 1000))
    assert abs(left[-1] - 0.125) < 1e-6
    offline.SetGain(1.0)
    track.SetFrequencyRatio(2.0)
    assert track.GetFrequencyRatio() == 2.0
    offline.SetFrequencyRatio(1.5)
    assert offline.GetFrequencyRatio() == 1.5


def test_spatial_and_channel_map(offline, mix):
    track = _const_track(mix, offline, left=0.5, right=0.25)
    track.Play()
    track.SetOutputChannelMap([1, 0])
    left, right = _channels(_generate(offline, 1000))
    assert set(left[100:]) == {0.25} and set(right[100:]) == {0.5}
    track.SetOutputChannelMap(None)
    left, right = _channels(_generate(offline, 1000))
    assert set(left[100:]) == {0.5} and set(right[100:]) == {0.25}
    track.SetStereo((0.0, 1.0))
    assert track.Get3DPosition() == (0.0, 0.0, 0.0)
    left, right = _channels(_generate(offline, 1000))
    assert _peak(left[100:]) == 0.0 and _peak(right[100:]) > 0.0
    track.SetOutputChannelMap(None)  # resets in forced-stereo mode too
    track.Set3DPosition((1.0, 0.0, -1.0))
    assert track.Get3DPosition() == (1.0, 0.0, -1.0)
    track.SetOutputChannelMap(None)  # ...and in 3D mode
    track.SetStereo(None)
    with pytest.raises(TypeError):
        track.SetStereo((1.0,))
    with pytest.raises(TypeError):
        track.Set3DPosition((1.0, 2.0))
    with pytest.raises(ValueError):
        track.SetOutputChannelMap([])
    with pytest.raises(mix.error):
        track.SetOutputChannelMap([0, 1, 2])  # wrong channel count


def test_tags(offline, mix):
    first = _const_track(mix, offline)
    second = _const_track(mix, offline)
    first.Tag('sfx')
    second.Tag('sfx')
    second.Tag('ui')
    assert sorted(second.GetTags()) == ['sfx', 'ui']
    assert {id(t) for t in offline.GetTaggedTracks('sfx')} == {id(first), id(second)}
    first.Untag('sfx')
    assert offline.GetTaggedTracks('sfx') == [second]
    assert offline.GetTaggedTracks('nope') == []
    offline.PlayTag('ui', loops=-1)
    assert second.Playing() and not first.Playing()
    offline.PauseTag('ui')
    assert second.Paused()
    offline.ResumeTag('ui')
    offline.SetTagGain('ui', 0.25)
    assert second.GetGain() == 0.25
    offline.StopTag('ui', fade_out_ms=0)
    assert not second.Playing()
    first.Play(loops=-1)
    offline.PauseAllTracks()
    assert first.Paused()
    offline.ResumeAllTracks()
    offline.StopAllTracks()
    assert not first.Playing()


def test_groups(offline, mix):
    group = offline.CreateGroup()
    other = mix.MixerGroup(offline)
    assert group.GetMixer() is offline and other.GetMixer() is offline
    track = _const_track(mix, offline)
    track.SetGroup(group)
    seen = []

    def silence(g, spec, data):
        seen.append((g, spec))
        return bytes(len(data))

    group.SetPostMixCallback(silence)
    track.Play(loops=-1)
    assert _peak(_channels(_generate(offline, 1000))[0]) == 0.0
    assert seen[0] == (group, (RATE, mix.AUDIO_F32, 2))
    group.SetPostMixCallback(None)
    assert _peak(_channels(_generate(offline, 1000))[0]) == 0.5
    track.SetGroup(None)
    group.Destroy()
    with pytest.raises(mix.error, match='MixerGroup'):
        track.SetGroup(group)
    with pytest.raises(mix.error, match='MixerGroup'):
        group.SetPostMixCallback(None)
    elsewhere = mix.Mixer(spec=(RATE, mix.AUDIO_F32, 2), device=None)
    try:
        with pytest.raises(mix.error, match='different mixers'):
            track.SetGroup(elsewhere.CreateGroup())
    finally:
        elsewhere.Destroy()
    with pytest.raises(TypeError):
        track.SetGroup('nope')


# --- callbacks --------------------------------------------------------------

def test_pcm_callbacks(offline, mix, capfd):
    track = _const_track(mix, offline)
    seen = []

    def raw(t, spec, data):
        seen.append(('raw', t, spec))
        return array.array('f', [x * 2 for x in array.array('f', data)]).tobytes()

    def cooked(t, spec, data):
        seen.append(('cooked', t))
        try:
            t.Stop()
        except mix.error as e:  # SDL forbids changing the track here
            seen.append(str(e))

    track.SetRawCallback(raw)
    track.SetCookedCallback(cooked)
    track.Play(loops=-1)
    left, right = _channels(_generate(offline, 1000))
    assert set(left) == {1.0} and set(right) == {0.5}
    assert ('raw', track, (RATE, mix.AUDIO_F32, 2)) in seen and ('cooked', track) in seen
    assert any('raw / cooked' in s for s in seen if isinstance(s, str))
    assert track.Playing()

    track.SetRawCallback(lambda t, spec, data: b'short')
    track.SetCookedCallback(lambda t, spec, data: 1 / 0)
    left, _ = _channels(_generate(offline, 256))
    assert set(left) == {0.5}  # bad returns leave the audio alone
    err = capfd.readouterr().err
    assert 'ValueError' in err and 'ZeroDivisionError' in err
    track.SetRawCallback(None)
    track.SetCookedCallback(None)
    with pytest.raises(TypeError):
        track.SetRawCallback(42)

    offline.SetPostMixCallback(lambda m, spec, data: _stereo(0.25, 0.75, len(data) // FRAME))
    left, right = _channels(_generate(offline, 256))
    assert set(left) == {0.25} and set(right) == {0.75}
    offline.SetPostMixCallback(None)


def test_stopped_callback_may_restart_the_track(offline, mix):
    track = _const_track(mix, offline, frames=100)
    runs = []

    def again(t):
        runs.append(t)
        if len(runs) < 5:
            t.Play()

    track.SetStoppedCallback(again)
    track.Play()
    _generate(offline, 2000)
    assert runs == [track] * 5


def test_destroy_and_quit_refused_in_callbacks(offline, mix):
    track = _const_track(mix, offline, frames=10)
    errors = []

    def stopped(t):
        for call in (offline.Destroy, mix.MIX_Quit, mix.MIX_Init):
            try:
                call()
            except mix.error as e:
                errors.append(str(e))

    track.SetStoppedCallback(stopped)
    track.Play()
    _generate(offline, 100)
    assert len(errors) == 3 and all('callback' in e for e in errors)
    assert offline.GetFormat()  # still alive


# --- inputs -----------------------------------------------------------------

def test_audio_stream_input(offline, mix):
    stream = mix.AudioStream(mix.AUDIO_F32, 2, RATE, mix.AUDIO_F32, 2, RATE)
    stream.Put(_stereo(0.5, 0.25, 2000))
    stream.Flush()
    track = offline.CreateTrack()
    track.SetAudioStream(stream)
    assert track.GetAudioStream() is stream and track.GetAudio() is None
    del stream
    gc.collect()  # the track keeps it alive
    track.Play()
    left, right = _channels(_generate(offline, 1000))
    assert set(left[100:]) == {0.5} and set(right[100:]) == {0.25}
    track.SetAudioStream(None)
    assert track.GetAudioStream() is None
    with pytest.raises(TypeError):
        track.SetAudioStream(b'nope')
    with pytest.raises(mix.error, match='AudioStream'):
        track.SetAudioStream(mix.AudioStream())


def test_io_stream_inputs(offline, mix, tmp_path):
    track = offline.CreateTrack()
    source = bytearray(_wav(200, rate=RATE))
    track.SetIOStream(source)
    source[:] = bytes(len(source))  # copied into a stream the mixer owns
    assert track.GetAudio() is None
    track.Play()
    assert _peak(_channels(_generate(offline, 2000))[0]) > 0.1

    path = tmp_path / 'tone.wav'
    path.write_bytes(_wav(200, rate=RATE))
    track.SetIOStream(path)
    track.Play()
    assert _peak(_channels(_generate(offline, 2000))[0]) > 0.1

    track.SetRawIOStream(_stereo(0.5, 0.25, 1000), (RATE, mix.AUDIO_F32, 2))
    track.Play()
    left, _ = _channels(_generate(offline, 500))
    assert set(left[100:]) == {0.5}
    track.SetIOStream(None)
    with pytest.raises(mix.error):
        track.Play()
    with pytest.raises(mix.error):
        track.SetIOStream(b'not audio')


# --- decoding ---------------------------------------------------------------

def test_decoder(mix, tmp_path):
    wav = _wav(100, rate=22050)
    decoder = mix.MixerDecoder(wav)
    assert decoder.GetFormat() == (22050, mix.AUDIO_S16, 1)
    chunks = []
    while chunk := decoder.Decode(1000):
        chunks.append(chunk)
    assert sum(map(len, chunks)) == 2205 * 2
    decoder.Destroy()
    with pytest.raises(mix.error, match='MixerDecoder'):
        decoder.Decode(10)
    decoder.Destroy()  # twice is fine

    path = tmp_path / 'tone.wav'
    path.write_bytes(wav)
    converted = mix.MixerDecoder(path, decoder='WAV').Decode(4096, (RATE, mix.AUDIO_F32, 2))
    assert len(converted) == 4096 and _peak(array.array('f', converted)) > 0.1
    with pytest.raises(ValueError):
        mix.MixerDecoder(wav).Decode(-1)
    with pytest.raises(mix.error):
        mix.MixerDecoder(b'not audio')


# --- lifetimes --------------------------------------------------------------

def test_tracks_keep_their_mixer_and_destroy_invalidates(mix):
    mixer = mix.Mixer(spec=(RATE, mix.AUDIO_F32, 2), device=None)
    track = mixer.CreateTrack()
    group = mix.MixerGroup(mixer)
    del mixer
    gc.collect()
    mixer = track.GetMixer()  # still alive through the track
    mixer.Destroy()
    for call in (track.Play, track.Playing, lambda: track.SetGroup(None), track.GetMixer,
                 lambda: group.SetPostMixCallback(None), mixer.GetFormat, mixer.CreateTrack):
        with pytest.raises(mix.error, match='not initialized'):
            call()
    mixer.Destroy()  # twice is fine
    with pytest.raises(mix.error):
        mix.MixerTrack(mixer)


def test_mix_quit_invalidates_everything(mix):
    mixer = mix.Mixer(spec=(RATE, mix.AUDIO_F32, 2), device=None)
    audio = mix.MIX_CreateSineWaveAudio(440, 0.5, 100)
    track = mixer.CreateTrack()
    track.SetAudio(audio)
    track.SetStoppedCallback(lambda t: None)
    decoder = mix.MixerDecoder(_wav())
    locked = mix.Mixer(spec=(RATE, mix.AUDIO_F32, 2), device=None)
    locked.Lock()  # this thread's own lock is released, not waited for
    mix.MIX_Quit()  # matches the fixture's MIX_Init: everything goes
    for call in (track.Play, audio.GetDuration, decoder.GetFormat, mixer.GetFormat, locked.GetFormat):
        with pytest.raises(mix.error, match='not initialized'):
            call()
    mix.MIX_Init()  # for the fixture's MIX_Quit


def test_lock_unlock(offline, mix):
    offline.Lock()
    offline.Lock()
    track = _const_track(mix, offline)  # recursive: calls work while locked
    track.Play()
    with pytest.raises(mix.error, match='Unlock'):
        offline.Destroy()
    offline.Unlock()
    offline.Unlock()
    with pytest.raises(mix.error, match='not locked'):
        offline.Unlock()
    offline.Lock()
    errors = []
    thread = threading.Thread(target=lambda: errors.append(pytest.raises(mix.error, offline.Unlock)))
    thread.start()
    thread.join()
    assert errors  # another thread cannot unlock it
    offline.Unlock()


# --- device mixers (real audio thread) --------------------------------------

def test_device_mixer_callbacks_run_off_thread(mix):
    mixer = mix.Mixer()
    try:
        assert mixer.GetDevice() > 0
        assert len(mixer.GetFormat()) == 3
        track = mixer.CreateTrack()
        track.SetAudio(mix.MIX_CreateSineWaveAudio(440, 0.1, 30))
        threads = []
        track.SetStoppedCallback(lambda t: threads.append((t, threading.get_ident())))
        track.Play()
        assert _wait(lambda: threads)
        assert threads[0][0] is track and threads[0][1] != threading.get_ident()
        restarts = []

        def again(t):
            restarts.append(1)
            if len(restarts) < 3:
                t.Play()  # re-enters the mixer from its own thread

        track.SetStoppedCallback(again)
        track.Play()
        assert _wait(lambda: len(restarts) == 3)
    finally:
        mixer.Destroy()


def test_callbacks_and_threads_do_not_deadlock(sdl):
    # Callbacks run on the audio thread holding the mixer lock and then need
    # the GIL; every binding call takes that lock with the GIL released, so
    # neither order can deadlock. Hammer it from two Python threads while the
    # audio thread runs stopped / raw / post-mix callbacks that call back in.
    if not hasattr(sdl, 'Mixer'):
        pytest.skip('built without SDL3_mixer')
    _run(sdl, '''
import gc, random, threading, time, SDL3
SDL3.Init(SDL3.INIT_AUDIO)
SDL3.MIX_Init()
mixer = SDL3.Mixer()
audio = SDL3.MIX_CreateSineWaveAudio(440, 0.05, 5)
tracks = []
for i in range(6):
    t = mixer.CreateTrack()
    t.SetAudio(audio)
    t.Tag('all')
    t.SetStoppedCallback(lambda tr: tr.Play() if random.random() < 0.9 else None)
    t.SetRawCallback(lambda tr, spec, data: data)
    tracks.append(t)
mixer.SetPostMixCallback(lambda m, spec, data: m.GetGain() and None)
stop = time.monotonic() + 1.5

def worker():
    while time.monotonic() < stop:
        t = random.choice(tracks)
        random.choice([t.Play, t.Stop, t.Pause, t.Resume, t.Playing,
                       lambda: t.SetGain(random.random()),
                       lambda: mixer.GetTaggedTracks('all'),
                       lambda: mixer.PlayTag('all')])()
        extra = mixer.CreateTrack()
        extra.SetAudio(audio)
        extra.Play()
        del extra  # destroyed while (maybe) mixing

threads = [threading.Thread(target=worker) for _ in range(2)]
for th in threads:
    th.start()
worker()
for th in threads:
    th.join()
for t in tracks:
    t.SetStoppedCallback(None)
del tracks
gc.collect()
mixer.Destroy()
SDL3.MIX_Quit()
SDL3.Quit()
print("ok")
''')


def test_mixer_freed_on_its_own_audio_thread(sdl):
    # The post-mix callback holds the Mixer object while it runs; if the main
    # thread drops its last reference meanwhile, the wrapper dies on the audio
    # thread, which must not free the mixer itself (it would join its own
    # thread): the free is handed to the main thread.
    if not hasattr(sdl, 'Mixer'):
        pytest.skip('built without SDL3_mixer')
    _run(sdl, '''
import threading, time, SDL3
SDL3.Init(SDL3.INIT_AUDIO)
SDL3.MIX_Init()
inside = threading.Event()
calls = [0]

def post_mix(m, spec, data):
    calls[0] += 1
    inside.set()
    time.sleep(0.05)  # lets the main thread drop its reference meanwhile

mixer = SDL3.Mixer()
mixer.SetPostMixCallback(post_mix)
assert inside.wait(5)
del mixer
deadline = time.monotonic() + 2
while time.monotonic() < deadline:
    before = calls[0]
    time.sleep(0.2)  # runs the pending free on this thread
    if calls[0] == before:
        break
else:
    raise AssertionError('the mixer kept mixing')
SDL3.MIX_Quit()
SDL3.Quit()
print("ok")
''')


def test_sdl_quit_releases_the_mixer_first(sdl):
    if not hasattr(sdl, 'Mixer'):
        pytest.skip('built without SDL3_mixer')
    _run(sdl, '''
import SDL3
SDL3.Init(SDL3.INIT_AUDIO)
SDL3.MIX_Init()
SDL3.MIX_Init()
mixer = SDL3.Mixer()
track = mixer.CreateTrack()
track.SetAudio(SDL3.MIX_CreateSineWaveAudio(440, 0.1, -1))
track.SetStoppedCallback(lambda t: None)
track.Play()
SDL3.Quit()  # no MIX_Quit: SDL3.Quit matches both MIX_Inits first
try:
    track.Play()
except SDL3.error:
    pass
else:
    raise AssertionError('track survived Quit')
SDL3.MIX_Quit()  # nothing left to match: a no-op
print("ok")
''')


def test_destroy_keeps_stream_inputs_until_the_mixer_is_gone(mix):
    # MIX_DestroyMixer still touches each track's AudioStream while stopping
    # it, so the binding releases a track's input only after the mixer is freed.
    for teardown in ('destroy', 'quit'):
        mixer = mix.Mixer(spec=(RATE, mix.AUDIO_F32, 2), device=None)
        track = mixer.CreateTrack()
        stream = mix.AudioStream(mix.AUDIO_F32, 2, RATE, mix.AUDIO_F32, 2, RATE)
        stream.Put(_stereo(0.5, 0.25, 4000))
        track.SetAudioStream(stream)
        del stream  # only the track holds it now
        track.Play()
        _generate(mixer, 100)
        if teardown == 'destroy':
            mixer.Destroy()
        else:
            mix.MIX_Quit()
            mix.MIX_Init()
        with pytest.raises(mix.error):
            track.Play()
