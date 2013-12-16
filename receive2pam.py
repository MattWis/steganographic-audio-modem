import math
import pyaudio
import sys
import struct
import numpy as np
import scipy.linalg
import matplotlib.pyplot as plt
import wave
import pickle
import os
import math
from lyrics import *
from constants import *

def legit_noise():
    np.random.seed(0)
    return np.random.randn(NOISE_SYMBOLS)

# only calculate once to preserve known-ness
legit_noise = legit_noise()

def randomData():
    np.random.seed(0)
    return np.sign(np.random.randn(DATA_SYMBOLS))

def d(length):
    return np.append(np.array([1]), np.zeros(length - 1)).astype(np.complex_)

def receive_filter():
    return raised_cosine(1)

def normalize(signal):
    return signal * (1.0 / max(signal))

def decode(received):
    zero_centered = received.astype(np.float64)
    real = zero_centered * cos(len(zero_centered)).astype(np.complex_)
    imag = zero_centered * sin(len(zero_centered)).astype(np.complex_)
    combined = real + (1j) * imag

    filtered = np.convolve(combined, receive_filter())
    sampled = np.zeros(len(filtered) / gap).astype(np.complex_)
    for i, zero in enumerate(sampled):
        sampled[i] = filtered[i * gap]
    return sampled[6:] # Based on number of zero-crossings in raised cosine

def demodulate_noise(noise):
    zero_centered = noise.astype(np.float64)
    demodulated = zero_centered * cos(len(zero_centered))
    normalized = normalize(demodulated)
    return np.convolve(normalized, receive_filter())

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

def generate_R_matrix(q):
    first_row = np.zeros(len(q) * 2).astype(np.complex_)
    first_row[0] = q[0]
    first_column = np.append(q, np.zeros(len(q) * 2 - 1))
    return np.matrix(scipy.linalg.toeplitz(first_column, first_row))

def use_white_noise_to_align(np_data):
    noise_band = np_data[:44100*2]
    channel = np.correlate(noise_band, legit_noise, "full")

    (maxIdx, maxVal) = maxIndex(abs(channel))
    delay = maxIdx + 1 - len(legit_noise)

    return np_data[delay + len(legit_noise):]

def get_least_squares_filter(noise_band, real_data):
    discrete_channel = np.correlate(noise_band, legit_noise[:ENCODED_NOISE], "full")

    (maxIdx, maxVal) = maxIndex(abs(discrete_channel))
    truncated_channel = discrete_channel[maxIdx - 10: maxIdx + 10]
    q = truncated_channel
    R = generate_R_matrix(q)
    desired = np.matrix(d(3 * len(q) - 1)).H
    return np.array(np.matrix(np.linalg.pinv(R) * desired).flat)

sound_file = wave.open("output.wav", 'r')
np_data = get_next_data_from_wav(sound_file, 10)

# dump_file = open('correlated.txt', 'r')
# channel = pickle.load(dump_file)

encoded_signal = use_white_noise_to_align(np_data)
data = decode(encoded_signal)

noise_band = data[:ENCODED_NOISE]
real_data = data[ENCODED_NOISE:]

least_squares_filter = get_least_squares_filter(noise_band, real_data)
equalized_data = np.convolve(least_squares_filter, real_data)

# print equalized_data
final_data = np.sign(equalized_data)
# print randomData()
print final_data[:DATA_SYMBOLS] - randomData()

for i in range(10):
    check_data = final_data[i:DATA_SYMBOLS + i]
    diff = check_data - randomData()
    num_wrong = 0
    for elem in diff:
        if elem != 0:
            num_wrong += 1
    print i, num_wrong

# print randomData()

plt.show()
