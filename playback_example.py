import math
import pyaudio
import sys
import struct
import numpy

p = pyaudio.PyAudio()
RATE = 44100

stream = p.open(format = pyaudio.paInt16,
                channels = 1,
                rate = RATE,
                input = True,
                output = True,
                frames_per_buffer = 1024)

data = stream.read(44100)

# Convert sound card data to numpy array
fmt = "%dH"%(len(data)/2)
data2 = struct.unpack(fmt, data)
np_data = numpy.array(data2, dtype='u2')

# Play back recorded sound
data = struct.pack(fmt, *list(np_data))

stream.write(data)

stream.stop_stream()
stream.close()

p.terminate()
