#!/usr/bin/env python3

# SDL3_mixer tour: three tagged tracks of sine tones - a panned bass drone, a
# lead fading in on the right, and an arpeggio whose stopped callback queues
# the next note - mixed through a group whose post-mix callback meters the
# level, then faded out by tag.
#
#   python3 example/mixer.py [--seconds N]      play on the default audio device
#   python3 example/mixer.py --offline out.wav  render through an offline mixer
#
# Needs the extension built with SDL3_mixer (brew install sdl3_mixer).

import argparse
import array
import math
import sys
import time
import wave

import SDL3

RATE = 48000
ARPEGGIO = [440, 554, 659, 880, 659, 554]


def build(mixer):
    group = mixer.CreateGroup()
    level = [0.0, False]  # RMS of the last mix, fading out

    def meter(group, spec, pcm):
        samples = array.array('f', pcm)
        level[0] = math.sqrt(sum(x * x for x in samples) / max(len(samples), 1))
        # returning None leaves the audio as it is

    group.SetPostMixCallback(meter)

    bass = mixer.CreateTrack()
    bass.SetAudio(SDL3.MIX_CreateSineWaveAudio(110, 0.25, -1, mixer))
    bass.SetStereo((1.0, 0.4))

    lead = mixer.CreateTrack()
    lead.SetAudio(SDL3.MIX_CreateSineWaveAudio(330, 0.15, -1, mixer))
    lead.SetStereo((0.3, 1.0))

    # Raw float PCM with a short attack / release (finite MIX_CreateSineWaveAudio
    # tones end on a NaN frame in SDL_mixer 3.2.4, which poisons the mix).
    def note(hz, ms=120, amp=0.2, edge=240):
        n = RATE * ms // 1000
        pcm = array.array('f', (amp * math.sin(2 * math.pi * hz * i / RATE) * min(1.0, i / edge, (n - i) / edge)
                                for i in range(n)))
        return SDL3.MIX_LoadRawAudio(pcm.tobytes(), (RATE, SDL3.AUDIO_F32, 1), mixer)

    notes = [note(hz) for hz in ARPEGGIO]
    arp = mixer.CreateTrack()
    step = [0]

    def next_note(track):
        # Runs on the mixing thread; restarting here leaves no gap. Once the
        # tag fade-out stopped it, it stays stopped.
        if level[1]:
            return
        step[0] = (step[0] + 1) % len(notes)
        track.SetAudio(notes[step[0]])
        track.Play()

    arp.SetAudio(notes[0])
    arp.SetStoppedCallback(next_note)

    for track in (bass, lead, arp):
        track.Tag('music')
        track.SetGroup(group)

    mixer.Lock()  # start bass and arpeggio on the same sample frame
    bass.Play()
    arp.Play()
    mixer.Unlock()
    lead.Play(fade_in_ms=1500, loops=-1)
    return (group, bass, lead, arp, notes), level


def play(seconds):
    mixer = SDL3.Mixer()
    print('device format:', mixer.GetFormat())
    keep, level = build(mixer)
    end = time.monotonic() + seconds
    faded = False
    while time.monotonic() < end:
        if not faded and end - time.monotonic() < 1.0:
            level[1] = True
            mixer.StopTag('music', fade_out_ms=900)
            faded = True
        bar = '#' * int(level[0] * 120)
        print(f'\r level {level[0]:.3f} {bar:<40}', end='', flush=True)
        time.sleep(0.05)
    print()
    mixer.Destroy()


def render(path, seconds):
    spec = (RATE, SDL3.AUDIO_S16, 2)
    mixer = SDL3.Mixer(spec=spec, device=None)
    keep, level = build(mixer)
    frames = int(RATE * seconds)
    chunk = RATE // 20
    with wave.open(path, 'wb') as out:
        out.setnchannels(2)
        out.setsampwidth(2)
        out.setframerate(RATE)
        done = 0
        faded = False
        while done < frames:
            if not faded and frames - done <= RATE:
                level[1] = True
                mixer.StopTag('music', fade_out_ms=900)  # once: calling again restarts the fade
                faded = True
            data, _ = mixer.Generate(min(chunk, frames - done) * 4)
            out.writeframes(data)
            done += len(data) // 4
    print(f'wrote {path}: {frames} frames, last level {level[0]:.3f}')
    mixer.Destroy()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--seconds', type=float, default=4.0)
    parser.add_argument('--offline', metavar='WAV', help='render to a WAV file instead of playing')
    args = parser.parse_args()

    if not hasattr(SDL3, 'Mixer'):
        sys.exit('this SDL3 build has no SDL3_mixer support')
    SDL3.Init(SDL3.INIT_AUDIO)
    SDL3.MIX_Init()
    print('SDL_mixer', '.'.join(map(str, SDL3.MIX_Version())), 'decoders:', ' '.join(SDL3.MIX_GetAudioDecoders()))
    try:
        if args.offline:
            render(args.offline, args.seconds)
        else:
            play(args.seconds)
    finally:
        SDL3.MIX_Quit()
        SDL3.Quit()


if __name__ == '__main__':
    main()
