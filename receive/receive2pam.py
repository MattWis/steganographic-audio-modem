import math
import pyaudio
import sys
import struct
import numpy as np
import matplotlib.pyplot as plt
import wave
import pickle
import os
import math

DATA_RATE = 200.0
PLAY_RATE = 44100.0
gap = int(PLAY_RATE / DATA_RATE)

def legit_noise():
    np.random.seed(0)
    return np.random.randn(44100 * 2)

# only calculate once to preserve known-ness
legit_noise = legit_noise()

def randomData():
    np.random.seed(0)
    return np.sign(np.random.randn(100))

def sinc():
    # Define sinc function
    raised_cosine(0)

def raised_cosine(beta = 0):
    width = PLAY_RATE / DATA_RATE * 6
    x = np.linspace(-width / 2 + 1, width / 2, width) * DATA_RATE / PLAY_RATE
    return np.sinc(x) * np.cos(math.pi * beta * x) / (1 - 4 * beta**2 * x**2)

def receive_filter():
    return raised_cosine(1)

def normalize(signal):
    return signal * (1.0 / max(signal))

def cos(length):
    x = np.linspace(1 / 10000.0, length / 10000.0, length)
    return np.cos(x * 2 * math.pi * 400)

def decode(received):
    zero_centered = received.astype(np.float64)
    demodulated = zero_centered * cos(len(zero_centered))
    normalized = normalize(demodulated)
    filtered = np.convolve(normalized, receive_filter())

    sampled = np.zeros(len(filtered) / gap)
    for i, zero in enumerate(sampled):
        sampled[i] = filtered[i * gap]
    return np.sign(sampled[6:]) # Based on number of zero-crossings in raised cosine

def maxIndex(list):
    maximum = max(list)
    for idx, val in enumerate(list):
        if val == maximum:
            return (idx, val)
    return (-1, maximum)

def get_next_data_from_wav(wav_file, num_seconds):
    raw_data = wav_file.readframes(44100 * num_seconds)

    # Convert sound card data to numpy array
    fmt = "%dh" % (len(raw_data)/2)
    data2 = struct.unpack(fmt, raw_data)
    return np.array(data2, dtype='int16')

sound_file = wave.open("output.wav", 'r')
np_data = get_next_data_from_wav(sound_file, 10)

# dump_file = open('correlated1.txt', 'r')
# channel = pickle.load(dump_file)

# dump_file = open('perfectChannelSendData.txt', 'r')
# np_data = pickle.load(dump_file)

noise_band = np_data[:44100*4]
channel = np.correlate(noise_band, legit_noise, "full")
# plt.plot(channel)
# plt.show()

(maxIdx, maxVal) = maxIndex(abs(channel))
delay = maxIdx + 1 - len(legit_noise)
print maxIdx, maxVal, delay

truncated_channel = channel[maxIdx - 200: maxIdx + 200]

encoded_signal = np_data[delay + len(legit_noise):]
data = decode(encoded_signal)
plt.plot(data)

print data[0:100] - randomData()

for i in range(10):
    check_data = data[i:100 + i]
    diff = check_data - randomData()
    num_wrong = 0
    for elem in diff:
        if elem != 0:
            num_wrong += 1
    print i, num_wrong

print randomData()


# impulse = channel[maxIdx:]
# plt.plot(correlated)

plt.show()
