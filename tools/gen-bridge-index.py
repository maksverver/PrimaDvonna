#!/usr/bin/env python

# Script to generate an index of neighbour bitmasks and whether not these
# correspond to possible bridges. A neighbour bitmask is a six-bit bitmask that
# specifies whether the six neighbours of a tile, in counter-clockwise order,
# are occupied or not. If all occupied neighbours are adjacent, then the
# current field cannot be a bridge field.
#
#   2   / \   1
#     /     \
#    |       |
#  3 |       | 0
#    |       |
#     \     /
#   4   \ /  5

def bridge(mask):
	tmp = mask
	if mask == 0b111111:
		groups = 1
	elif mask == 0:
		groups = 0
	else:
		# rotate so left-most bit is 1 and right-most bit is 0:
		while (mask&1) or not (mask&0b100000):
			mask = (mask>>1)|((mask&1)<<5)
		groups = 0
		while mask:
			while not mask&1: mask >>= 1
			while mask&1: mask >>= 1
			groups += 1
	return groups > 1

data = [ str(int(bridge(i))) for i in range(64) ]
print("static bool bridge_index[1<<6] = {")
print("\t" + ", ".join(data[ 0:16]) + ",")
print("\t" + ", ".join(data[16:32]) + ",")
print("\t" + ", ".join(data[32:48]) + ",")
print("\t" + ", ".join(data[48:64]) + " };")
