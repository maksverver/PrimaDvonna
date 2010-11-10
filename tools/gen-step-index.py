#!/usr/bin/env python2

H, W = 5, 11

blocked = [
	[ 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1 ],
	[ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 ],
	[ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ],
	[ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 ],
	[ 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 ] ]

DR = [  0, -1, -1,  0, +1, +1 ]
DC = [ +1,  0, -1, -1,  0, +1 ]

steps_index = {}

def get_steps(n, r1, c1):
	steps = []
	for dr, dc in zip(DR, DC):
		r2, c2 = r1 + n*dr, c1 + n*dc
		if n > 0 and not blocked[r1][c1] and \
			r2 >= 0 and r2 < H and c2 >= 0 and c2 < W and not blocked[r2][c2]:
			steps.append(W*(r2 - r1) + (c2 - c1))
	return tuple(steps)

def print_steps_data():
	all_steps = list(sorted(set([ get_steps(n, r1, c1)
			for c1 in range(W) for r1 in range(H) for n in range(50) ])))
	size = sum(map(lambda s: len(s) + 1, all_steps))
	print 'static const int a[' + str(size) + '] = {'
	values = []
	pos = 0
	for steps in all_steps:
		steps_index[steps] = pos
		for v in list(steps) + [0]:
			if len(values) == 16:
				print '\t' + ', '.join(map(lambda v: '%3d'%v, values)) + ','
				values = []
			values.append(v)
			pos += 1
	print '\t' + ', '.join(map(lambda v: '%3d'%v, values)) + ' };'
	assert pos == size

def print_steps_index():
	print 'const int *board_steps[50][5][11] = {'
	for n in range(50):
		print '\t{ /* ' + str(n) + ' */'
		for r1 in range(H):
			values = []
			for c1 in range(W):
				values.append(steps_index[get_steps(n, r1, c1)])
			line = "\t\t{ "
			line += ', '.join(map(lambda v: "a+%3d"%v, values))
			line += " }"
			if r1 + 1 < H:
				line += ','
			else:
				line += ' }'
				if n + 1 < 50:
					line += ','
				else:
					line += ' };'
			print line

print_steps_data()
print
print_steps_index()
