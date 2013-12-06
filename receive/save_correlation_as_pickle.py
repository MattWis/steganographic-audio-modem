import math
import struct
import numpy as np
import wave
import pickle
import os

DATA_RATE = 44.1
PLAY_RATE = 44100.0

def legit_noise():
    np.random.seed(0)
    return np.random.randn(44100*15)

data = wave.open("output.wav", 'r').readframes(44100 * 6)

# # Convert sound card data to numpy array
fmt = "%dh" % (len(data)/2)
data2 = struct.unpack(fmt, data)
np_data = np.array(data2, dtype='int16')

correlated = np.correlate(np_data.astype(np.float32), legit_noise(), "full")
dump_file = open('correlated.txt', 'w')
pickle.dump(correlated, dump_file)

