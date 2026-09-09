#!/usr/bin/env python3

import sys
import os
import math
import random
import struct
import time

import SDL3
import pygl as GL

SAMPLES=4096


class Hertz:
    def __init__(self, frequency):
        self.x = 0
        self.frequency(frequency)
        self.amplify(1.0)

    def frequency(self, freq):
        self.freq = freq
        self.unit = self.freq / 48000.0
        return self

    def amplify(self, factor):
        self.amp = (2**15 * factor) - 1
        return self

    def step(self):
        self.x += self.unit
        self.y = math.sin(math.tau * self.x)
        self.y = int(self.amp * self.y)
        return self.y


class Noise:
    def __init__(self):
        self.x = 0
        self.unit = 300 / 48000.0
        self.amplify(1.0)

    def frequency(self, freq):
        self.freq = freq
        self.unit = self.freq / 48000.0
        return self

    def amplify(self, factor):
        self.amp = (2**15 * factor) - 1
        return self

    def step(self):
        self.x += self.unit + (self.unit * (random.random() * 10))
        self.y = math.sin(math.tau * self.x)# + (random.random() * 0.8 - 0.4)
        self.y = int(self.amp * self.y)
        return self.y


class Audio(SDL3.Audio):
    def __init__(self):
        super(Audio, self).__init__()
        self.gl = []
        self.fresh = False
        self.generators = [
            Hertz(random.randrange(100,3000)).amplify(0.8),
            Hertz(48000 / (SAMPLES / 10.0)).amplify(0.5),
            Noise().amplify(0.5),
            ]

    def Open(self, name):
        super(Audio, self).Open(name, freq=48000, channels=1, samples=SAMPLES, flags=0, callback=self.callback)

    def callback(self, size, userData=None):
        self.fresh = True
        self.gl = []
        buf = []
        for x in range(SAMPLES):
            y = int(sum(h.step() for h in self.generators) / len(self.generators))
            self.gl.append(y)
            p = struct.pack('<h', y)
            buf.append(p[0])
            buf.append(p[1])
            #buf.append(p[2])
            #buf.append(p[3])
        return bytes(buf)


def main():
    SDL3.Init()

    audioDevices = SDL3.GetNumAudioDevices()
    if 1 == len(sys.argv):
        for idx in range(audioDevices):
            print(idx, SDL3.GetAudioDeviceName(idx))
        sys.exit(0)

    index = int(sys.argv[1])
    if index > audioDevices:
        print('Out of range')
        sys.exit(0)

    SDL3.GL_SetAttribute(SDL3.GL_DOUBLEBUFFER, 0)
    SDL3.GL_SetAttribute(SDL3.GL_BUFFER_SIZE, 32)

    SDL3.GL_SetAttribute(SDL3.GL_CONTEXT_MAJOR_VERSION, 2);
    SDL3.GL_SetAttribute(SDL3.GL_CONTEXT_MINOR_VERSION, 1);

    flags,width,height,rate = SDL3.GetDesktopDisplayMode(0)
    print('  ', f'Display Mode: {width}x{height} @ {rate}Hz')

    dc = SDL3.Window("Audio", size=(width,height), flags=SDL3.WINDOW_OPENGL | SDL3.WINDOW_FULLSCREEN_DESKTOP)
    dc.GL_CreateContext()
    dc.GL_MakeCurrent()
    GL.glewInit()

    SDL3.GL_SetSwapInterval(1)

    GL.ClearColor(0.01, 0.01, 0.01, 0.0)
    GL.ShadeModel(GL.SMOOTH)

    GL.Enable(GL.BLEND)
    GL.BlendFunc(GL.SRC_ALPHA, GL.ONE_MINUS_SRC_ALPHA)

    GL.Enable(GL.POINT_SMOOTH);
    GL.Hint(GL.POINT_SMOOTH_HINT, GL.FASTEST)

    GL.Enable(GL.LINE_SMOOTH);
    GL.Hint(GL.LINE_SMOOTH_HINT, GL.FASTEST)

    GL.Viewport(0, 0, width, height)

    GL.MatrixMode(GL.PROJECTION)
    GL.LoadIdentity()

    GL.Ortho(0.0, width, height, 0.0, 0.0, 1.0)

    GL.MatrixMode(GL.MODELVIEW)
    GL.LoadIdentity()


    GL.PointSize(4.0)

    GL.Clear(GL.COLOR_BUFFER_BIT)
    GL.Finish()
    #dc.GL_SwapWindow()
    #GL.Clear(GL.COLOR_BUFFER_BIT)
    #dc.GL_SwapWindow()

    r = 0.0
    g = 0.0
    b = 0.5

    rdir = 1
    gdir = 1
    bdir = 1

    name = SDL3.GetAudioDeviceName(index)
    print('Opening', name )
    audio = SDL3.Audio()
    audio.Open(name)

    points = 1
    paused = 0

    quit = False
    while not quit:
        while True:
            event = SDL3.PollEvent()
            if not event:
                break
            event,data = event

            if SDL3.EVENT_QUIT == event:
                quit = True
                break

            elif SDL3.EVENT_MOUSE_MOTION == event:
                audio.generators[0].frequency(data[1] + 100)
                audio.generators[1].frequency(data[2] * 4 + 100)
                audio.generators[2].frequency(data[2] * 4 + 100)
                r = data[1] / width
                g = data[2] / height

            elif SDL3.EVENT_KEY_DOWN == event:
                if SDL3.GetModState() & (SDL3.KMOD_LSHIFT | SDL3.KMOD_RSHIFT):
                    mult = 0.1
                else:
                    mult = 1
                # quit on escape
                if SDL3.K_ESCAPE == data[2]:
                    quit = True
                    break

                if SDL3.K_P == data[2]:
                    audio.Pause(paused)
                    paused = 0 if paused else 1

                if SDL3.K_Q == data[2]:
                    points = 0 if points else 1

                if SDL3.K_D == data[2]:
                    f = audio.generators[0].freq - 1 * mult
                    audio.generators[0].frequency(f)
                if SDL3.K_F == data[2]:
                    f = audio.generators[0].freq + 1 * mult
                    audio.generators[0].frequency(f)
                if SDL3.K_G == data[2]:
                    if audio.generators[0].amp == -1:
                        audio.generators[0].amplify(1.0)
                    else:
                        audio.generators[0].amplify(0)

                if SDL3.K_C == data[2]:
                    f = audio.generators[1].freq - 1 * mult
                    audio.generators[1].frequency(f)
                if SDL3.K_V == data[2]:
                    f = audio.generators[1].freq + 1 * mult
                    audio.generators[1].frequency(f)
                if SDL3.K_B == data[2]:
                    if audio.generators[1].amp == -1:
                        audio.generators[1].amplify(1.0)
                    else:
                        audio.generators[1].amplify(0)

                if SDL3.K_n == data[2]:
                    if audio.generators[2].amp == -1:
                        audio.generators[2].amplify(0.3)
                    else:
                        audio.generators[2].amplify(0)

        if audio.fresh:
            if len(audio.gl) < SAMPLES:
                continue

            GL.Clear(GL.COLOR_BUFFER_BIT)
            GL.Begin(GL.POINTS if points else GL.LINE_STRIP)

            r1 = r
            g1 = g
            b1 = b

            for idx,value in enumerate(audio.gl):
                x = idx * (width / SAMPLES)
                y = (value + 2**15) / 2**16
                y1 = y * height
                GL.Color4f(r1, g1, b1, 1.0)
                GL.Vertex3f(x, y1, 0.0)

            GL.End()
            GL.Finish()

            if rdir == 1:
                if r >= 1.0: rdir = -1
            else:
                if r <= 0: rdir = 1

            if gdir == 1:
                if g >= 1.0: gdir = -1
            else:
                if g <= 0: gdir = 1

            if bdir == 1:
                if b >= 1.0: bdir = -1
            else:
                if b <= 0: bdir = 1

            audio.fresh = False

    #audio.Close()

    SDL3.Quit()

if '__main__' == __name__:
    main()
