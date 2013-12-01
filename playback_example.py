import math
import pyaudio
import sys

p = pyaudio.PyAudio()
RATE = 44100

stream = p.open(format = p.get_format_from_width(1),
                channels = 1,
                rate = RATE,
                input = True,
                output = True,
                frames_per_buffer = 1024)

data = stream.read(44100)
print [ord(x) for x in data]

stream.write(data)

stream.stop_stream()
stream.close()

p.terminate()
