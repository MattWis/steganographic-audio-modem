from ctypes import *
import numpy as np
import math
import matplotlib.pyplot as plt

def make_test_mask():
	# Make a fake signal, and then calculate the mask generated
	MP1 = CDLL('/home/kevin/ADcomm/steganographic-audio-modem/MPEG1_encoding/PsychoMpegOne.so')
	MP1.PsychoMpegOne.restype = c_long
	MP1.PsychoMpegOne.argtypes = [ \
		POINTER(c_short), c_long,  \
		c_long, \
		c_long, \
		POINTER(c_double), POINTER(c_double), \
		POINTER(c_double), c_long, \
		POINTER(c_double), c_long, \
		POINTER(c_double)
		]

	fs = 44100
	t = np.arange(0, 1024, dtype=np.int16)
	time = t/float(fs)
	fake_data = np.ones(time.size)*2**15    \
		+ 2**14*np.sin(2*math.pi*4400*time) \
		+ 2**14*np.sin(2*math.pi*1000*time) \
		+ 0.5*2**14*np.random.randn(time.size)
	# plt.plot(time, fake_data)
	# plt.show()

	short_data  = fake_data.ctypes.data_as(POINTER(c_double))
	data = np.ctypeslib.as_array(short_data, fake_data.shape)
	# plt.plot(time, data)
	# plt.show()
	print fake_data
	print "=========================="
	print data


	bufferlen   = c_long(1024)
	fs 			= c_long(fs)
	fft_size	= c_long(1024)
	spectrum	= POINTER(c_double)
	masking		= POINTER(c_double)
	spectrum_eff 	= POINTER(c_double)
	spectrum_range  = c_long() 

if __name__ == '__main__':
	make_test_mask()
