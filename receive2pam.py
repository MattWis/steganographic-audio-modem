import math
import pyaudio
import sys
import struct
import numpy as np
import matplotlib.pyplot as plt
import wave

DATA_RATE = 44.1
PLAY_RATE = 44100.0

def legit_noise():
    np.random.seed(0)
    return np.random.randn(44100)

def sinc():
    # Define sinc function
    width = 200.0
    x = np.linspace(-width / 2, width / 2, width * PLAY_RATE / DATA_RATE)
    return np.sinc(x)

def pulse():
    return (sinc() * (2**13)).astype(np.int16)

def normalize(signal):
    return signal / max(signal)

def decode(received):
    zero_centered = received.astype(np.float64) - 2**15
    normalized = normalize(zero_centered)

    # plt.plot(np.convolve(normalized, sinc()))
    # plt.plot(zero_centered)
    # plt.show()

def maxIndex(list):
    maximum = max(list)
    for idx, val in enumerate(list):
        if val == maximum:
            return (idx, val)
    return (-1, maximum)

# data = np.sign(np.random.randn(100))
# decode(received)

# p = pyaudio.PyAudio()
# RATE = 44100

# stream = p.open(format = pyaudio.paInt16,
                # channels = 1,
                # rate = RATE,
                # input = True,
                # output = True,
                # frames_per_buffer = 1024)

# data = stream.read(88200)
data = wave.open("output.wav", 'r').readframes(88200)

# # Convert sound card data to numpy array
fmt = "%dH" % (len(data)/2)
data2 = struct.unpack(fmt, data)
np_data = np.array(data2, dtype='u2')

correlated = np.correlate(np_data.astype(np.float64), legit_noise(), "full")
(maxIdx, maxVal) = maxIndex(abs(correlated))
print maxIdx, maxVal
impulse = correlated[maxIdx:]

channel = np.fft.fft(impulse)
freq = np.fft.fftfreq(impulse.shape[-1])
plt.plot(freq, channel.real, 'k')
plt.plot(freq, channel.imag, 'b')
plt.show()


# # Close pyAudio
# stream.stop_stream()
# stream.close()

# p.terminate()
