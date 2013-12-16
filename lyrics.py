def get_bits(string):
    integers = map(ord, string)
    bitstring = ''.join(format(ord(x), '0>8b') for x in string)
    output = []
    for bit in bitstring:
        if bit == '0':
            output.append(-1)
        elif bit == '1':
            output.append(1)
    return output

lyrics = """My power flurries through the air into the ground
My soul is spiralling in frozen fractals all around
And one thought crystallizes like an icy blast
I'm never going back, the past is in the past"""

data = get_bits(lyrics)

def get_string(bits):
    bytes_ = []
    for i in range(len(bits) / 8):
        bytes_.append(bits[i * 8:(i + 1) * 8])
    integers = []
    for byte in bytes_:
        integers.append(get_int(byte))
        # print chr(get_int(byte))
    return ''.join(map(chr, integers))

def get_int(byte):
    integer = 0
    for i, bit in enumerate(byte):
        if bit == 1:
            integer += 2**(7 - i)
    return integer
            
