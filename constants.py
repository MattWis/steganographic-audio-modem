import numpy as np
import math

DATA_RATE = 50.0
PLAY_RATE = 44100.0
gap = int(PLAY_RATE / DATA_RATE)
NOISE_SYMBOLS = 88200
ENCODED_NOISE = 200
DATA_SYMBOLS = 200

def raised_cosine(beta = 0, width = PLAY_RATE / DATA_RATE * 6):
    x = np.linspace(-width / 2 + 1, width / 2, width) * DATA_RATE / PLAY_RATE
    y = np.sinc(x) * np.cos(math.pi * beta * x) / (1 - 4 * beta**2 * x**2)
    one_half = np.where(x == .5)
    y[one_half] = .5
    minus_one_half = np.where(x == -.5)
    y[minus_one_half] = .5
    return y

def sinc():
    raised_cosine(0)

def cos(length):
    x = np.linspace(1 / PLAY_RATE, length / PLAY_RATE, length)
    return np.cos(x * 2 * math.pi * 800)

def sin(length):
    x = np.linspace(1 / PLAY_RATE, length / PLAY_RATE, length)
    return np.sin(x * 2 * math.pi * 800)
