from ctypes import *
import numpy as np
import math
import matplotlib.pyplot as plt
from decimal import Decimal
import timeit

MP1 = CDLL('/home/kevin/ADcomm/steganographic-audio-modem/MPEG1_encoding/PsychoMpegOne.so')
spec_out = np.zeros(1024)
mask_out = np.zeros(1024)
meff_out = np.zeros(1024)
spef_out = np.zeros(1024)


def make_fake_data():
	# Make a fake signal, and then calculate the mask generated
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
	fake_data = np.ones(time.size)*2**14    \
		+ 2**13*np.sin(2*math.pi*10000*time) \
		+ 2**13*np.sin(2*math.pi*5000*time) \
		+ 0.1*2**13	*np.random.randn(time.size)
	fake_data = fake_data.astype(np.int16)
	return fake_data

def make_test_mask(samples):
	# Set appropriate variables for the function call
	short_data  	= samples.ctypes.data_as(POINTER(c_short))
	bufferlen   	= c_long(1024)
	fs 				= c_long(44100)
	fft_size		= c_long(1024)
	spectrum		= spec_out.ctypes.data_as(POINTER(c_double))
	masking			= mask_out.ctypes.data_as(POINTER(c_double))
	spectrum_eff 	= meff_out.ctypes.data_as(POINTER(c_double))
	spectrum_range  = c_long(32)
	masking_eff		= spef_out.ctypes.data_as(POINTER(c_double))
	masking_range 	= c_long(32)
	smr_eff			= np.ones(1024).ctypes.data_as(POINTER(c_double))

	# calculate the psychoacoustic masking threshold
	dummy = MP1.PsychoMpegOne (short_data, bufferlen,  \
		    fs, \
		    fft_size,  \
		    spectrum, masking, \
		    spectrum_eff, spectrum_range,  \
		    masking_eff, masking_range, \
		    smr_eff \
		)

	final_mask	= np.ctypeslib.as_array(masking, samples.shape)
	final_spec	= np.ctypeslib.as_array(spectrum, samples.shape)
	final_spef	= np.ctypeslib.as_array(spectrum_eff, samples.shape)
	final_meff	= np.ctypeslib.as_array(masking_eff, samples.shape)

	freqs = np.linspace(0, fs.value, 1024)
	# plt.plot(final_mask, color='g')
	# plt.plot(final_spec, color='b')
	# plt.plot(freqs[0:513], final_meff[0:513], color='g')
	# plt.plot(freqs[0:513], final_spef[0:513], color='b')
	# plt.show()


if __name__ == '__main__':
	samples = make_fake_data()
	make_test_mask(samples)
	# timeit.timeit("make_test_mask(samples)", setup="from __main__ import make_test_mask, samples", number=10)