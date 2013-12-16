import math
import pyaudio
import sys
import struct
import numpy as np
from scipy.io import wavfile
import matplotlib.pyplot as plt
import pickle

DATA_RATE = 44.1
PLAY_RATE = 44100.0

def legit_noise():
    np.random.seed(0)
    return np.random.randn(44100*2) * 5000

def sinc():
    # Define sinc function
    width = DATA_RATE
    x = np.linspace(-width / 2, width / 2, width * PLAY_RATE / DATA_RATE)
    return np.sinc(x)

def pulse():
    return sinc()

def createRandomData(freq, seed=0):
    np.random.seed(seed)
    data = np.sign(np.random.randn(100))
    return encode(data, freq)

def encode(bits, freq):

    gap = int(PLAY_RATE / DATA_RATE)
    wave = np.zeros(gap * len(bits) + len(pulse()))
    x = np.linspace(1/10000.0,len(wave)/10000.0,len(wave))

    cos = np.cos(x*2*math.pi*freq)

    for i, bit in enumerate(bits):
        deadtime = np.zeros(gap * i)
        convolved = np.append(deadtime, pulse() * bit)
        convolved.resize(len(wave))
        wave += convolved

    unsigned_wave = (wave*cos)*10000
    return unsigned_wave.astype(np.int16)
    #.astye(np.uint16)

p = pyaudio.PyAudio()
RATE = 44100

stream = p.open(format = pyaudio.paInt16,
                channels = 1,
                rate = RATE,
                input = True,
                output = True,
                frames_per_buffer = 1024)

# Package white noise + data
randData1 = createRandomData(1200)
randData2 = createRandomData(800, seed=2)
legitNoise = legit_noise()

# Load a song and add that in
(fs, song) = wavfile.read('Frozen.wav')

avsong = np.sum(song, axis=1)/2
package = 0.1*np.append(legitNoise,randData1 + randData2)
package += avsong[:len(package)]

# Package just white noise
#package = legitNoise

# Package just data
#package = randData

# Plot data
x1 = np.linspace(1,len(package), len(package)) 
# plt.plot(x1,package)
plt.show()

# Save to pickle
dump_file = open('perfectChannelSendData.txt', 'w')
pickle.dump(package, dump_file)

# Play data
fmt = "%dh" % (len(package))
print max(package)
print min(package)
data = struct.pack(fmt, *list(package))
stream.write(data)

stream.stop_stream()
stream.close()

p.terminate()