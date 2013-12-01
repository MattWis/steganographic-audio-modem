import math
import pyaudio
import sys
import struct
import numpy as np
import matplotlib.pyplot as plt

DATA_RATE = 100.0
PLAY_RATE = 44100.0

def sinc():
    # Define sinc function
    width = 200.0
    x = np.linspace(-width / 2, width / 2, width * PLAY_RATE / DATA_RATE)
    return (np.sinc(x) * (1.7 * 2**13)).astype(np.int16)


def encode (bits):

    gap = int(PLAY_RATE / DATA_RATE)

    wave = np.zeros(gap * len(bits) + len(sinc()), np.int16)

    for i, bit in enumerate(bits):
        deadtime = np.zeros(gap * i, np.int16)
        convolved = np.append(deadtime, sinc() * bit)
        convolved.resize(len(wave))
        wave += convolved
        
    print max(wave)
    aligned_wave = (wave + 2**15).astype(np.uint16)



    plt.plot(aligned_wave)
    plt.plot(sinc())
    plt.show()


    
encode(np.sign(np.random.randn(100)))

# p = pyaudio.PyAudio()
# RATE = 44100

# stream = p.open(format = p.paInt16,
                # channels = 1,
                # rate = RATE,
                # input = True,
                # output = True,
                # frames_per_buffer = 1024)

# data = stream.read(44100)

# # Convert sound card data to numpy array
# fmt = "%dH" % (len(sinc()))
# data2 = struct.unpack(fmt, data)
# np_data = np.array(data2, dtype='u2')

# # Play back recorded sound
# data = struct.pack(fmt, *list(sinc()))
# stream.write(data)

# stream.stop_stream()
# stream.close()

# p.terminate()
