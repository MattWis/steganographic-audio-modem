import math
import pyaudio
import sys
import struct
import numpy as np
import matplotlib.pyplot as plt

DATA_RATE = 44.1
PLAY_RATE = 44100.0

def legit_noise():
    np.random.seed(0)
    return (np.random.randn(44100) * 10000 + 2**15).astype(np.uint16)

def sinc():
    # Define sinc function
    width = 200.0
    x = np.linspace(-width / 2, width / 2, width * PLAY_RATE / DATA_RATE)
    return np.sinc(x)

def pulse():
    return (sinc() * (2**13)).astype(np.int16)

def encode(bits):

    gap = int(PLAY_RATE / DATA_RATE)
    wave = np.zeros(gap * len(bits) + len(pulse()), np.int16)

    for i, bit in enumerate(bits):
        deadtime = np.zeros(gap * i, np.int16)
        convolved = np.append(deadtime, pulse() * bit)
        convolved.resize(len(wave))
        wave += convolved
        
    unsigned_wave = (wave + 2**15).astype(np.uint16)
    return unsigned_wave


p = pyaudio.PyAudio()
RATE = 44100

stream = p.open(format = pyaudio.paInt16,
                channels = 1,
                rate = RATE,
                input = True,
                output = True,
                frames_per_buffer = 1024)


# Play encoded sound
fmt = "%dH" % (len(legit_noise()))
data = struct.pack(fmt, *list(legit_noise()))
stream.write(data)

stream.stop_stream()
stream.close()

p.terminate()
