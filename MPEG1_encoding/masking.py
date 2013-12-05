from ctypes import *
import numpy as np
import math
import matplotlib.pyplot as plt

def make_test_mask():
	# Make a fake signal, and then calculate the mask generated
	MP1 = CDLL('/home/kevin/ADcomm/steganographic-audio-modem/MPEG1_encoding/PsychoMpegOne.so')
	
	fs = 44100
	t = np.arange(0, 0.5*fs, dtype=np.int16)
	time = t/float(fs)
	print type(time)
	fake_data = np.ones(time.size)*2**15 \
		+ 2**14*np.sin(2*math.pi*440*time) \
		+ 2**14*np.sin(2*math.pi*100*time) \
		+ 0.5*2**14*np.random.randn(time.size)
	plt.plot(time, fake_data)
	plt.show()

if __name__ == '__main__':
	make_test_mask()
