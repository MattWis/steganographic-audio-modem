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

DATA_RATE = 44.1
PLAY_RATE = 44100.0
gap = int(PLAY_RATE / DATA_RATE) + 1

def legit_noise():
    np.random.seed(0)
    return np.random.randn(44100*15)

# only calculate once to preserve known-ness
legit_noise = legit_noise()

def sinc():
    # Define sinc function
    width = DATA_RATE
    x = np.linspace(-width / 2, width / 2, width * PLAY_RATE / DATA_RATE)
    return np.sinc(x)

def receive_filter():
    return sinc()

def normalize(signal):
    return signal * (1.0 / max(signal))

def cos(length):
    x = np.linspace(1, length / 100, length)
    return np.cos(x * 2 * math.pi * 4)

def decode(received):
    zero_centered = received.astype(np.float64) - 2**15
    demodulated = received * cos(len(received))
    normalized = normalize(demodulated)
    filtered = np.convolve(normalized, receive_filter())

    sampled = np.zeros(len(filtered) / gap)
    for i, zero in enumerate(bits):
        sampled[i] = filtered[i * gap]
    return sampled

    # plt.plot(np.convolve(normalized, sinc()))
    # plt.plot(zero_centered)
    # plt.show()

def maxIndex(list):
    maximum = max(list)
    for idx, val in enumerate(list):
        if val == maximum:
            return (idx, val)
    return (-1, maximum)


def smooth(x,window_len=11):
    """smooth the data using a window with requested size.
    
    This method is based on the convolution of a scaled window with the signal.
    The signal is prepared by introducing reflected copies of the signal 
    (with the window size) in both ends so that transient parts are minimized
    in the begining and end part of the output signal.
    
    input:
        x: the input signal 
        window_len: the dimension of the smoothing window; should be an odd integer
        window: the type of window from 'flat', 'hanning', 'hamming', 'bartlett', 'blackman'
            flat window will produce a moving average smoothing.
 
    output:
        the smoothed signal
        
    example:
 
    t=linspace(-2,2,0.1)
    x=sin(t)+randn(len(t))*0.1
    y=smooth(x)
    
    see also: 
    
    np.hanning, np.hamming, np.bartlett, np.blackman, np.convolve
    scipy.signal.lfilter
 
    TODO: the window parameter could be the window itself if an array instead of a string
    NOTE: length(output) != length(input), to correct this: return y[(window_len/2-1):-(window_len/2)] instead of just y.
    """
    if x.ndim != 1:
        raise ValueError, "smooth only accepts 1 dimension arrays."
    if x.size < window_len:
        raise ValueError, "Input vector needs to be bigger than window size."
    if window_len<3:
        return x
 
    s=np.r_[x[window_len-1:0:-1],x,x[-1:-window_len:-1]]
    
    w = np.hanning(window_len)
    # w = np.ones(window_len, 'd')
 
    y=np.convolve(w/w.sum(),s,mode='valid')
    return y

def get_next_data_from_wav(wav_file, num_seconds):
    raw_data = wav_file.readframes(44100 * num_seconds)

    # Convert sound card data to numpy array
    fmt = "%dh" % (len(raw_data)/2)
    data2 = struct.unpack(fmt, raw_data)
    return np.array(data2, dtype='int16')

sound_file = wave.open("output.wav", 'r')
np_data = get_next_data_from_wav(sound_file, 10)

dump_file = open('correlated1.txt', 'r')
correlated = pickle.load(dump_file)

(maxIdx, maxVal) = maxIndex(abs(correlated))
delay = maxIdx - len(legit_noise)
print maxIdx, maxVal, delay

encoded_signal = np_data[delay + gap * len(legit_noise):]
print encoded_signal

# impulse = correlated[maxIdx:]
# plt.plot(correlated)
# plt.show()

# channel = np.fft.fft(impulse)
# smooth_channel = smooth(channel, 500)
# freq = np.fft.fftfreq(impulse.shape[-1])
# plt.plot(smooth(freq * 44100, 500), smooth(np.sqrt(channel.real**2 + channel.imag**2), 500), 'k')
# plt.show()
