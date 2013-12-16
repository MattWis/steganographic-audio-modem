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

DATA_RATE = 300.0
PLAY_RATE = 44100.0
gap = int(PLAY_RATE / DATA_RATE)
NOISE_SYMBOLS = 88200
ENCODED_NOISE = 500
DATA_SYMBOLS = 475

lyrics = """My power flurries from the air into the ground
My soul is spiralling in frozen fractals all around
And one thought crystallizes like an icy blast
I'm never going back, the past is in the past"""

def get_bits(string):
    integers = map(ord, string)
    bitstring = ''.join(format(ord(x), '0>8b') for x in string)
    output = []
    for bit in bitstring:
        if bit == '0':
            output.append(-1)
        elif bit == '1':
            output.append(1)
    return output

bits = get_bits(lyrics)

def legit_noise():
    np.random.seed(0)
    return np.random.randn(88200)

# only calculate once to preserve known-ness
legit_noise = legit_noise()

def randomData():
    np.random.seed(0)
    return np.sign(np.random.randn(DATA_SYMBOLS))

def sinc():
    # Define sinc function
    raised_cosine(0)

def raised_cosine(beta = 0, width = PLAY_RATE / DATA_RATE * 6):
    x = np.linspace(-width / 2 + 1, width / 2, width) * DATA_RATE / PLAY_RATE
    y = np.sinc(x) * np.cos(math.pi * beta * x) / (1 - 4 * beta**2 * x**2)
    one_half = np.where(x == .5)
    y[one_half] = .5
    minus_one_half = np.where(x == -.5)
    y[minus_one_half] = .5
    return y

def d(length):
    return np.append(np.array([1]), np.zeros(length - 1)).astype(np.complex_)

def receive_filter():
    return raised_cosine(1)

def normalize(signal):
    return signal * (1.0 / max(signal))

def cos(length):
    x = np.linspace(1 / 10000.0, length / 10000.0, length)
    return np.cos(x * 2 * math.pi * 400)

def sin(length):
    x = np.linspace(1 / 10000.0, length / 10000.0, length)
    return np.sin(x * 2 * math.pi * 400)

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

sound_file = wave.open("output.wav", 'r')
np_data = get_next_data_from_wav(sound_file, 10)

# dump_file = open('correlated.txt', 'r')
# channel = pickle.load(dump_file)

# dump_file = open('perfectChannelSendData.txt', 'r')
# np_data = pickle.load(dump_file)

noise_band = np_data[:44100*2]
channel = np.correlate(noise_band, legit_noise, "full")

(maxIdx, maxVal) = maxIndex(abs(channel))
delay = maxIdx + 1 - len(legit_noise)
print maxIdx, maxVal, delay

encoded_signal = np_data[delay + len(legit_noise):]
data = decode(encoded_signal)

# print data
# plt.plot(data.real)
# plt.plot(data.imag)
# plt.show()

noise_band = data[:ENCODED_NOISE]
discrete_channel = np.correlate(noise_band, legit_noise[:ENCODED_NOISE], "full")
# plt.plot(discrete_channel.real)
# plt.plot(discrete_channel.imag)
# plt.show()

print data
real_data = data[ENCODED_NOISE:]

# plt.plot(real_data.real, 'b')
# plt.plot(real_data.imag, 'k')
# plt.plot(randomData() * 20000, 'g')
# plt.show()

(maxIdx, maxVal) = maxIndex(abs(discrete_channel))
truncated_channel = discrete_channel[maxIdx - 10: maxIdx + 10]
q = truncated_channel
R = generate_R_matrix(q)
d = np.matrix(d(3 * len(q) - 1)).H
least_squares_filter = np.array(np.matrix(np.linalg.pinv(R) * d).flat)


print "LSF"
# plt.plot(least_squares_filter.real)
# plt.plot(least_squares_filter.imag)
# plt.show()

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
