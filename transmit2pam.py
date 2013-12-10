import math
import pyaudio
import sys
import struct
import numpy as np
import matplotlib.pyplot as plt
import pickle

DATA_RATE = 200.0
PLAY_RATE = 44100.0

def legit_noise():
    np.random.seed(0)
    return np.random.randn(44100*2) * 5000

def sinc():
    # Define sinc function
    width = DATA_RATE
    x = np.linspace(-width / 2+1, width / 2, width) *  DATA_RATE / PLAY_RATE
    return np.sinc(x)

def raised_cosine(beta = 0):
    width = PLAY_RATE / DATA_RATE * 6
    x = np.linspace(-width / 2 + 1, width / 2, width) * DATA_RATE / PLAY_RATE
    return np.sinc(x) * np.cos(math.pi * beta * x) / (1 - 4 * beta**2 * x**2)

def pulse():
    return raised_cosine(1)

def createRandomData():
    np.random.seed(0)
    data = np.sign(np.random.randn(100))
    print data
    return encode(data)

def encode(bits):

    gap = int(PLAY_RATE / DATA_RATE)
    wave = np.zeros(gap * len(bits) + len(pulse()))
    x = np.linspace(1/10000.0,len(wave)/10000.0,len(wave))

    cos = np.cos(x*2*math.pi*400)

    for i, bit in enumerate(bits):
        deadtime = np.zeros(gap * i)
        convolved = np.append(deadtime, pulse() * bit)
        convolved.resize(len(wave))
        wave += convolved   
        
    unsigned_wave = (wave*cos)*10000
    return unsigned_wave.astype(np.int16)

p = pyaudio.PyAudio()
RATE = 44100

stream = p.open(format = pyaudio.paInt16,
                channels = 1,
                rate = RATE,
                input = True,
                output = True,
                frames_per_buffer = 1024)

# Package white noise + data
randData = createRandomData()
legitNoise = legit_noise()
package =  np.append(legitNoise,randData)

# Package just white noise
#package = legitNoise

# Package just data
#package = randData

# Plot data
#x1 = np.linspace(1,len(package), len(package)) 
#plt.plot(x1,package)
#lt.show()

# Save to pickle
#dump_file = open('perfectChannelSendData.txt', 'w')
#pickle.dump(package, dump_file)

raw_input("Press enter to start playing data")

# Play data
fmt = "%dh" % (len(package))
data = struct.pack(fmt, *list(package))
stream.write(data)

stream.stop_stream()
stream.close()

p.terminate()