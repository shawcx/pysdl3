#!/usr/bin/env python3

# Load a WAV (or synthesise a chord), resample it to the device's native format
# with SDL3.AudioStream, and play it through the queue. No callback, no window.
#
#   python3 example/wav.py [file.wav]

import math
import struct
import sys
import time
import SDL3

SDL3.Init(SDL3.INIT_AUDIO)

print('audio driver:', SDL3.GetCurrentAudioDriver())

if len(sys.argv) > 1:
    (src_rate, src_fmt, src_chan, _samples), pcm = SDL3.LoadWAV(sys.argv[1])
    print(f'loaded {sys.argv[1]}: {src_rate} Hz, {src_chan} ch, format {src_fmt:#06x}, {len(pcm)} bytes')
else:
    src_rate, src_fmt, src_chan = 22050, SDL3.AUDIO_S16, 1
    frames = src_rate  # one second
    pcm = bytearray()
    for i in range(frames):
        t = i / src_rate
        env = min(1.0, 8 * t, 8 * (1 - t))
        s = sum(math.sin(2 * math.pi * f * t) for f in (261.63, 329.63, 392.00)) / 3
        pcm += struct.pack('<h', int(env * 24000 * s))
    pcm = bytes(pcm)
    print('synthesised a 1s C-major chord')

# Open the device; let SDL pick a native format and see what we got.
device = SDL3.Audio()
dev_rate, dev_fmt, dev_chan, dev_samples = device.Open('', channels=2)
print(f'device: {dev_rate} Hz, {dev_chan} ch, format {dev_fmt:#06x}')

if (src_rate, src_fmt, src_chan) != (dev_rate, dev_fmt, dev_chan):
    stream = SDL3.AudioStream(src_fmt, src_chan, src_rate, dev_fmt, dev_chan, dev_rate)
    stream.Put(pcm)
    stream.Flush()
    pcm = stream.Get(stream.Available())
    print(f'resampled to {len(pcm)} bytes')

device.Queue(pcm)
device.Pause(0)
print('playing...', end=' ', flush=True)

while device.GetQueueSize() > 0:
    time.sleep(0.05)

# let the last buffer drain
time.sleep(2 * dev_samples / dev_rate)
device.Close()
SDL3.Quit()
print('done')
