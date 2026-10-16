'''Audio: the callback trampoline (phase 0), plus drivers, queueing, LoadWAV,
mixing, and AudioStream resampling (phase 8).'''

import struct
import time

import pytest


@pytest.fixture
def device_name(sdl):
    if sdl.GetNumAudioDevices() < 1:
        pytest.skip('no audio playback device')
    return sdl.GetAudioDeviceName(0)


def _make_wav(pcm, rate=8000):
    hdr = b'RIFF' + struct.pack('<I', 36 + len(pcm)) + b'WAVE'
    hdr += b'fmt ' + struct.pack('<IHHIIHH', 16, 1, 1, rate, rate * 2, 2, 16)
    hdr += b'data' + struct.pack('<I', len(pcm))
    return hdr + pcm


def _run(audio, seconds=0.15):
    audio.Pause(0)
    time.sleep(seconds)
    audio.Pause(1)


def test_playback_callback_is_invoked(sdl, device_name):
    calls = []

    def callback(size, userdata):
        calls.append((size, userdata))
        return b'\x00' * size

    audio = sdl.Audio()
    freq, fmt, channels, samples = audio.Open(
        device_name, freq=48000, channels=1, samples=1024, callback=callback, userdata='tag')
    assert freq == 48000
    _run(audio)
    audio.Close()

    assert calls, 'callback never fired'
    assert calls[0][1] == 'tag'


def test_reopen_after_close(sdl, device_name):
    audio = sdl.Audio()
    audio.Open(device_name, freq=48000, channels=1, samples=512, callback=lambda s, u: b'\x00' * s)
    audio.Close()
    # Would raise "Audio device already open" if Close failed to reset state.
    audio.Open(device_name, freq=48000, channels=1, samples=512, callback=lambda s, u: b'\x00' * s)
    audio.Close()


def test_bad_callback_return_does_not_crash(sdl, device_name, capfd):
    audio = sdl.Audio()
    audio.Open(device_name, freq=48000, channels=1, samples=512, callback=lambda s, u: 'not bytes')
    _run(audio, 0.1)
    audio.Close()
    # the trampoline reports the error on stderr instead of propagating it
    assert 'must return bytes' in capfd.readouterr().err


# --- phase 8 --------------------------------------------------------------

def test_open_default_device(sdl):
    # no deviceName -> the system default device
    audio = sdl.Audio()
    spec = audio.Open(freq=44100, channels=2, samples=1024)
    assert spec[0] == 44100
    audio.Close()


def test_driver_module_functions(sdl):
    n = sdl.GetNumAudioDrivers()
    assert n >= 1
    assert all(sdl.GetAudioDriver(i) for i in range(n))
    assert sdl.GetCurrentAudioDriver() == 'dummy'


def test_queue_status_and_clear(sdl, device_name):
    audio = sdl.Audio()
    audio.Open(device_name, freq=44100, format=sdl.AUDIO_S16, channels=2, samples=2048)
    assert audio.GetStatus() == sdl.AUDIO_PAUSED
    audio.Pause(0)
    assert audio.GetStatus() == sdl.AUDIO_PLAYING

    audio.Queue(b'\x01\x02' * 500)
    assert audio.GetQueueSize() == 1000
    audio.ClearQueued()
    assert audio.GetQueueSize() == 0
    audio.Close()


def test_load_wav_from_bytes_and_path(sdl, tmp_path):
    pcm = struct.pack('<200h', *[(i * 137) % 20000 - 10000 for i in range(200)])
    wav = _make_wav(pcm)

    spec, data = sdl.LoadWAV(wav)
    assert len(spec) == 3 and spec[0] == 8000
    assert data == pcm

    path = tmp_path / 'tone.wav'
    path.write_bytes(wav)
    spec2, data2 = sdl.LoadWAV(str(path))
    assert data2 == pcm


def test_mix_audio_format(sdl):
    a = struct.pack('<4h', 1000, 2000, 3000, 4000)
    b = struct.pack('<4h', 500, 500, 500, 500)

    full = struct.unpack('<4h', sdl.MixAudioFormat(a, b, sdl.AUDIO_S16, 1.0))
    assert full == (1500, 2500, 3500, 4500)

    half = struct.unpack('<4h', sdl.MixAudioFormat(a, b, sdl.AUDIO_S16, 0.5))
    assert half == (1250, 2250, 3250, 4250)


def test_audio_stream_resamples(sdl):
    stream = sdl.AudioStream(sdl.AUDIO_S16, 1, 22050, sdl.AUDIO_F32, 2, 44100)
    src = struct.pack('<100h', *([12000, -12000] * 50))
    stream.Put(src)
    stream.Flush()

    available = stream.Available()
    # up-sample 2x, mono->stereo 2x, s16 (2 bytes) -> f32 (4 bytes) 2x  => ~8x
    assert len(src) * 6 <= available <= len(src) * 10

    out = stream.Get(available)
    assert len(out) == available
    assert stream.Available() == 0

    stream.Clear()
    stream.Put(src)
    stream.Flush()  # a resampling stream may hold short input back until flushed
    assert stream.Available() > 0
    stream.Clear()
    assert stream.Available() == 0


def test_audio_stream_get_returns_what_is_available(sdl):
    stream = sdl.AudioStream(sdl.AUDIO_S16, 1, 8000, sdl.AUDIO_S16, 1, 8000)
    stream.Put(b'\x00\x01' * 10)
    stream.Flush()
    out = stream.Get(1_000_000)  # ask for far more than exists
    assert 0 < len(out) <= 40


def test_uninitialised_audio_stream_is_guarded(sdl):
    with pytest.raises(sdl.error):
        sdl.AudioStream().Put(b'\x00')
    with pytest.raises(sdl.error):
        sdl.AudioStream().Available()
