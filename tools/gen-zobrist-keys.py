#!/usr/bin/env python2

import struct

N,H,W = 49,5,11

def keys():
	fp = file('/dev/urandom', 'rb')
	while True:
		yield "0x%016xull" % struct.unpack('=Q', fp.read(8))

it = keys()

print 'static const hash_t field_key[4*(N+1)][H][W] = {'
for n in range(4*(N+1)):
	print '\t{ /* ' + str(n) + ' */'
	for r in range(H):
		values = []
		for c in range(W):
			values.append(it.next())
		line = "\t\t{ "
		line += ', '.join(values)
		line += " }"
		if r + 1 < H:
			line += ','
		else:
			line += ' }'
			if n + 1 < 4*(N+1):
				line += ','
			else:
				line += ' };'
		print line
print 'static const hash_t player_key = ' + it.next() + ';'
print 'static const hash_t phase_key = ' + it.next() + ';'
print 'static const hash_t init_key = ' + it.next() + ';'
