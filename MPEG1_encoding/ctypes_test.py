from ctypes import *
import numpy

MP1 = CDLL('/home/kevin/ADcomm/steganographic-audio-modem/MPEG1_encoding/PsychoMpegOne.so')
MP1.hz2bark.restype = c_double
MP1.bark2hz.restype = c_double

hz = MP1.bark2hz(c_double(9))
bark = MP1.hz2bark(c_double(1000))

print "Hz:", hz
print "bark:", bark
