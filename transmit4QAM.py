import math
import pyaudio
import sys
import struct
import numpy as np
import matplotlib.pyplot as plt
import pickle
from constants import *
import lyrics

def legit_noise():
    np.random.seed(0)
    return np.random.randn(NOISE_SYMBOLS) * 5000

def encoded_legit_noise():
    np.random.seed(0)
    data = np.sign(np.random.randn(DATA_SYMBOLS))
    np.random.seed(0)
    noise = np.random.randn(ENCODED_NOISE)
    
    return encodeCos(np.append(noise, data))

def pulse():
    return raised_cosine(1)

def createRandomData():
    np.random.seed(0)
    data = np.sign(np.random.randn(100))
    
    return encodeCos(data)

def encode_data():
    np.random.seed(0)
    noise = np.random.randn(ENCODED_NOISE*2)
    np.random.seed(0)
    data = lyrics.data
    #data = np.sign(np.random.randn(DATA_SYMBOLS*2))

    halfData = int(len(data)/2)
    halfNoise = int(len(noise)/2)

    cosData = encodeCos(np.append(noise[:halfNoise],data[:halfData]))
    sinData = encodeSin(np.append(noise[halfNoise:2*halfNoise],data[halfData:2*halfData]))

    return  cosData + sinData

def encodeSin(bits):

    wave = np.zeros(gap * len(bits) + len(pulse()))
 
    sine = sin(len(wave))
    return (encode(wave, bits)*sine).astype(np.int16)

def encodeCos(bits):

    wave = np.zeros(gap * len(bits) + len(pulse()))
 
    cosine = cos(len(wave))
    return (encode(wave, bits)*cosine).astype(np.int16)

def encode(wave, bits):

    for i, bit in enumerate(bits):
        deadtime = np.zeros(gap * i)
        convolved = np.append(deadtime, pulse() * bit)
        convolved.resize(len(wave))
        wave += convolved   
        
    unsigned_wave = wave*10000
    return unsigned_wave

p = pyaudio.PyAudio()
RATE = 44100

stream = p.open(format = pyaudio.paInt16,
                channels = 1,
                rate = RATE,
                input = True,
                output = True,
                frames_per_buffer = 1024)

# Create white noise + data
randData = createRandomData()
legitNoise = legit_noise()
encodedNoise = encoded_legit_noise()

encodedData = encode_data()

package = np.append(legitNoise, encodedData)

# Plot data
#x1 = np.linspace(1,len(package), len(package)) 
##plt.plot(x1,package)
#plt.show()

# Save to pickle
#dump_file = open('perfectChannelSendData.txt', 'w')
#pickle.dump(package, dump_file)

fmt = "%dh" % (len(package))
data = struct.pack(fmt, *list(package))

raw_input("Press enter to start playing data")

# Play data
stream.write(data)

stream.stop_stream()
stream.close()

p.terminate()