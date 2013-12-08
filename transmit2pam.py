import math
import pyaudio
import sys
import struct
import numpy as np
import matplotlib.pyplot as plt
import pickle

DATA_RATE = 44.1
PLAY_RATE = 44100.0

def legit_noise():
    np.random.seed(0)
    return (np.random.randn(44100*2) * 10000 + 2**15).astype(np.uint16)

def sinc():
    # Define sinc function
    width = DATA_RATE
    x = np.linspace(-width / 2, width / 2, width * PLAY_RATE / DATA_RATE)
    return np.sinc(x)

def pulse():
    return (sinc() * 2**10).astype(np.int16)

def createRandomData():
    np.random.seed(0)
    print np.random.randn(3)
    data = np.sign(np.random.randn(100))
    #print data
    return encode(data)

def encode(bits):

    gap = int(PLAY_RATE / DATA_RATE)
    wave = np.zeros(gap * len(bits) + len(pulse()), np.int16)
    x = np.linspace(1/10000.0,len(wave)/10000.0,len(wave))
    print len(wave)

    cos = np.cos(x*2*math.pi*400) * .01

    for i, bit in enumerate(bits):
        deadtime = np.zeros(gap * i, np.int16)
        convolved = np.append(deadtime, pulse() * bit)
        convolved.resize(len(wave))
        wave += convolved
        
    unsigned_wave = ((((wave + 2**15)*cos))+400)*3
    return unsigned_wave.astype(np.uint16)
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
randData = createRandomData()
legitNoise = legit_noise()
package =  np.append(legitNoise,randData)

# Package just white noise
#package = legitNoise

# Package just data
#package = randData

x = np.linspace(1,44100/10, 44100)
cos = (np.cos(x) * 2**10 + 2**15)
print cos

# Plot data
#x1 = np.linspace(1,len(randData), len(randData)) 
#plt.plot(x1,randData)
#plt.show()

dump_file = open('perfectChannelSendData.txt', 'w')
pickle.dump(package, dump_file)

# Play data
fmt = "%dH" % (len(package))
data = struct.pack(fmt, *list(package))
stream.write(data)

stream.stop_stream()
stream.close()

p.terminate()