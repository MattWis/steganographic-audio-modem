import math
import pyaudio
import sys

p = pyaudio.PyAudio()
RATE = 44100
WAVE = 4000
T = 1
data = [int(math.sin(x * math.pi * WAVE / RATE) * 127 + 128) for x in xrange(RATE * T)]
print data

stream = p.open(format = p.get_format_from_width(1),
                channels = 1,
                rate = RATE,
                input = True,
                output = True,
                frames_per_buffer = 1024)

stream.write(''.join([chr(x) for x in data]))

stream.stop_stream()
stream.close()

p.terminate()
