#!/usr/bin/env python2

id_rc = ( [ (0, c) for c in range(0,  9) ] +
		  [ (1, c) for c in range(0, 10) ] +
		  [ (2, c) for c in range(0, 11) ] +
		  [ (3, c) for c in range(1, 11) ] +
		  [ (4, c) for c in range(2, 11) ] )

rc_id = {}
for i, (r,c) in enumerate(id_rc): rc_id[r,c] = i

DR = [  0, -1, -1,  0, +1, +1 ]
DC = [ +1,  0, -1, -1,  0, +1 ]

steps_index = {}

def get_steps(n, r1, c1):
	steps = []
	for dr, dc in zip(DR, DC):
		r2, c2 = r1 + n*dr, c1 + n*dc
		if n > 0 and (r1,c1) in rc_id and (r2,c2) in rc_id:
			steps.append(rc_id[r2,c2] - rc_id[r1,c1])
	return tuple(steps)

def print_steps_data():
	all_steps = list(sorted(set([
		get_steps(n, r, c) for (r,c) in id_rc for n in range(50) ])))
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
	print '\t' + ', '.join(map(lambda v: '%3d'%v, values))
	print '};'
	assert pos == size

def print_steps_index():
	print 'const int *board_steps[50][49] = {'
	for n in range(50):
		values = []
		line = "\t{ "
		for i, (r,c) in enumerate(id_rc):
			line += "a+%3d"%steps_index[get_steps(n, r, c)]
			if i + 1 < len(id_rc):
				if (i+1)%10 == 0:
					line += ",\n\t  "
				else:
					line += ", "
		line += ' }'
		if n + 1 < 50:
			line += ','
		line += '  /* ' + str(n) + ' */'
		print line
	print '};'

def print_distance():
	print 'const char board_distance[49][49] = {'
	for f, (r1,c1) in enumerate(id_rc):
		values = []
		line = "\t{     "
		for g, (r2,c2) in enumerate(id_rc):
			dx = c2 - c1
			dy = r2 - r1
			dz = dx - dy
			dist = max(max(abs(dx), abs(dy)), abs(dz))
			line += "%2d" % dist

			if g + 1 < len(id_rc):
				if id_rc[g + 1][0] != r2:
					line += ",\n\t  " + abs(id_rc[g + 1][0] - 2)*"  "
				else:
					line += ", "
		line += ' }'
		if f + 1 < len(id_rc):
			line += ','
		line += '  /* ' + str(f) + ' */'
		print line
	print '};'

def print_neighbours():
	code = 'const char board_neighbours[49] = {\n\t    '
	for f, (r,c) in enumerate(id_rc):
		mask = 0
		for i, (dr, dc) in enumerate(zip(DR, DC)):
			if (r + dr, c + dc) in rc_id: mask |= 1 << i
		code += '%2d'%mask
		if f + 1 < len(id_rc):
			code += ", "
			if id_rc[f + 1][0] != r:
				code += "\n\t" + abs(id_rc[f + 1][0] - 2)*"  "
		else:
			code += '\n};'
	print code

print_steps_data()
print
print_steps_index()
print
print_distance()
print
print_neighbours()
