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
