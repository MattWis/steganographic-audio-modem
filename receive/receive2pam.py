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
    return np.random.randn(44100 * 2)

# only calculate once to preserve known-ness
legit_noise = legit_noise()

def randomData():
    np.random.seed(0)
    return np.sign(np.random.randn(100))

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
    x = np.linspace(1 / 10000.0, length / 10000.0, length)
    return np.cos(x * 2 * math.pi * 400)

def decode(received):
    zero_centered = received.astype(np.float64)
    demodulated = received * cos(len(received))
    normalized = normalize(demodulated)
    filtered = np.convolve(normalized, receive_filter())

    sampled = np.zeros(len(filtered) / gap)
    for i, zero in enumerate(sampled):
        sampled[i] = filtered[i * gap]
    return np.sign(sampled[44:-44])

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

# sound_file = wave.open("output.wav", 'r')
# np_data = get_next_data_from_wav(sound_file, 10)

# dump_file = open('correlated1.txt', 'r')
# channel = pickle.load(dump_file)

dump_file = open('perfectChannelSendData.txt', 'r')
np_data = pickle.load(dump_file)

noise_band = np_data[0:44100*3]
channel = np.correlate(noise_band, legit_noise, "full")

(maxIdx, maxVal) = maxIndex(abs(channel))
delay = maxIdx + 1 - len(legit_noise)
print maxIdx, maxVal, delay

encoded_signal = np_data[delay + len(legit_noise):]
data = decode(encoded_signal)
print data - randomData()
plt.plot(data)

# impulse = channel[maxIdx:]
# plt.plot(correlated)
# plt.show()

# channel = np.fft.fft(impulse)
# smooth_channel = smooth(channel, 500)
# freq = np.fft.fftfreq(impulse.shape[-1])
# plt.plot(smooth(freq * 44100, 500), smooth(np.sqrt(channel.real**2 + channel.imag**2), 500), 'k')
plt.show()
