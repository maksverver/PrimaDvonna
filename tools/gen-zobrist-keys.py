#!/usr/bin/env python2

import random

N,H,W = 49,5,11

def keys():
	while True:
		yield "" % random.getrandbits(64)

it = keys()

num_field_keys = 4*(N+1)*H*W
print 'static const unsigned long long zobrist_init_key = 0x%016xull;' % random.getrandbits(64)
print 'static const unsigned long long zobrist_player_key = 0x%016xull;' % random.getrandbits(64)
print 'static const unsigned long long zobrist_phase_key = 0x%016xull;' % random.getrandbits(64)
print 'static const unsigned zobrist_field_key[4*(N+1)*H*W] = {'
for n in range(num_field_keys//W):
	values = []
	for c in range(W):
		values.append("0x%08xu" % random.getrandbits(32))
	print "\t " + ', '.join(values) + (" };", ",")[n + 1 < num_field_keys//W]
