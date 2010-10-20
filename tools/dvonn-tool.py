#!/usr/bin/env python

import re, sys, optparse

# Base-62 digits used to encode board states
digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"

# Board dimensions (height H and width W) and number of valid fields:
H, W, N = 5, 11, 49

# Player names:
NONE  = -1
WHITE = 0
BLACK = 1

# Game phases:
PLACEMENT = 0
MOVEMENT  = 1
COMPLETE  = 2

# Movement directions:
D = zip( [ -1, -1,  0,  0, +1, +1 ],
         [ -1,  0, -1, +1,  0, +1 ] )

def initialize():
	# Index mapping strings like "A1" to coordinates like (0,0)
	global fields_by_id
	fields_by_id = {}
	for x in range(W):
		for y in range(H):
			if valid_field(x, y):
				fields_by_id[field_str(x, y)] = (x, y)

class Field():
	'Models a single field on the board.'

	def __init__(self, x, y, player = -1, pieces = 0, dvonn = False):
		self.x      = x
		self.y      = y
		self.player = player
		self.pieces = pieces
		self.dvonn  = dvonn

	def place(dst, turn):
		assert dst.pieces == 0
		dst.pieces = 1
		if turn < 3:
			dst.dvonn  = True
		else:
			dst.player = turn%2

	def move(src, dst):
		assert src.pieces > 0 and dst.pieces > 0
		dst.player = src.player
		dst.pieces = src.pieces + dst.pieces
		dst.dvonn  = src.dvonn or dst.dvonn
		src.clear()

	def clear(self):
		self.player = -1
		self.pieces = 0
		self.dvonn  = False

	def clone(self):
		return Field(self.x, self.y, self.player, self.pieces, self.dvonn) 

	def __repr__(self):
		return "+- "[self.player] + ".123456789abcdefghijklmnopqrstuvwxyz"[self.pieces] + " *"[self.dvonn]


def clone_board(board):
	return [ [ field.clone() for field in row ] for row in board ]

def distance(x1, y1, x2, y2):
	'Returns the distance between fields at coordinates (x1,y1) and (x2,y2)'
	dx = x2 - x1
	dy = y2 - y1
	dz = dy - dx
	return max(abs(dx), abs(dy), abs(dz))

def valid_field(x, y):
	'Returns whether coordinates (x,y) refer to a valid field on the board.'
	return distance(x, y, W//2, H//2) <= max(H, W)//2

def field_str(x, y):
	'Returns the fields coordinates (x,y) as a standard string.'
	return chr(ord('A') + x) + chr(ord('1') + y)

def game_phase(moves, complete = False):
	'Returns the current game phase for the given move history.'
	if isinstance(moves, list): moves = len(moves)
	if complete: return COMPLETE
	if moves < N: return PLACEMENT
	else: return MOVEMENT

def next_player(moves):
	'''Returns the next player to move for the given move history.
		`moves' is either a list or an integer.'''
	if isinstance(moves, list): moves = len(moves)
	if moves < N: return moves%2
	else: return (moves - N)%2

def parse_move(token):
	if token == 'RESIGN' or token == 'PASS':
		return None
	if len(token) == 2:
		return fields_by_id[token]
	if len(token) == 4:
		return fields_by_id[token[0:2]], fields_by_id[token[2:4]]
	assert False

def parse_transcript(data):
	'''Parses a transcript in Little Golem's format and returns a dictionary
	with the game's metadata and a list of moves.'''
	tokens = list(re.findall('\[[^]]*\]|[^\s]+', data))
	metadata, moves, pos = {}, [], 0
	while pos + 1 < len(tokens) and tokens[pos + 1][0] == '[':
		metadata[tokens[pos]] = tokens[pos + 1][1:-1]
		pos += 2
	while pos < len(tokens):
		token = tokens[pos].upper()
		pos += 1
		if next_player(moves) == 0:
			# skip move number
			assert token == str(1 + len(moves)//2) + '.'
			token = tokens[pos].upper()
			pos += 1
		moves.append(parse_move(token))
		if len(moves) + 1 == N:
			fields = set(fields_by_id.values())
			for m in moves: fields.remove(m)
			assert len(fields) == 1
			moves.append(list(fields)[0])
	return metadata, moves

def format_transcript(metadata, moves, complete):
	def format_key(key):
		return ''.join([c for c in key if c.isalnum() ])
	def format_value(value):
		return '[' + value.replace('[', '').replace(']', '') + ']'
	res = ''
	for key in metadata:
		key = format_key(key)
		if key:
			if res: res += ' '
			res += key + ' ' + format_value(metadata[key])
	for i in range(len(moves)):
		if i + 1 == N: continue
		if res: res += ' '
		if next_player(i) == 0:
			res += str(i//2 + 1) + '. '
		if moves[i] is None:
			if i + 1 == len(moves) and complete:
				res += "resign"
			else:
				res += "pass"
		elif i < N:
			res += field_str(*moves[i]).lower()
		else:
			res += field_str(*moves[i][0]).lower() + field_str(*moves[i][1]).lower()
	return res

def format_move(move):
	if move is None: return "PASS"
	if isinstance(move[0], tuple):
		return field_str(*move[0]) + field_str(*move[1])
	else:
		return field_str(*move)

def parse_logfile(data):
	# N.B. metadata not parsed!
	moves = []
	lines = data.split("\n")
	for line in lines:
		line = line.strip()
		if line == "": continue
		if line[0] == '#': continue
		for token in line.split():
			moves.append(parse_move(token))
	return {}, moves

def format_logfile(metadata, moves):
	# N.B. metadata is ignored!
	res = ""
	line = ""
	for i, move in enumerate(moves):
		if line != "": line += " "
		line += format_move(move)
		if i == 23 or i == 48 or (i >= N and (i - N)%16 == 15):
			res += line + "\n"
			line = ""
	if line != "": res += line + "\n"
	return res

def format_dvonner(metadata, moves):
	# N.B. metadata is ignored!
	moves = map(format_move, moves)
	moves.append("BREAK")
	res = ""
	line = ""
	for i in range(len(moves)):
		if line: line += "  "
		line += moves[i]
		if (i + 1 < N and i%2 == 1) or (i + 1 >= N and i%2 == 0):
			res += line + "\n"
			line = ""
	if line: res += line + "\n"
	return res

def mobile(board, x1, y1):
	'Returns whether the stack at coordinates (x1,y1) can be moved by a player.'
	if board[x1][y1].player < 0: return False
	for dx, dy in D:
		x2, y2 = x1 + dx, y1 + dy
		if x2 < 0 or x2 >= W or y2 < 0 or y2 >= H or board[x2][y2].pieces == 0:
			return True
	return False

def all_moves(board):
	'Generates a list of possible moves for either player on the board.'
	moves = []
	for x1 in range(W):
		for y1 in range(H):
			if mobile(board, x1, y1):
				n = board[x1][y1].pieces
				for dx, dy in D:
					x2, y2 = x1 + dx*n, y1 + dy*n
					if 0 <= x2 < W and 0 <= y2 < H and board[x2][y2].pieces > 0:
						moves.append(((x1,y1), (x2,y2)))
	return moves

def remove_disconnected(board):
	'Removes pieces which are not connected to Dvonn pieces from the board.'
	used = [ [ False for y in range(H) ] for x in range(W) ]
	def dfs(x1, y1):
		if used[x1][y1]: return
		used[x1][y1] = True
		for dx, dy in D:
			x2, y2 = x1 + dx, y1 + dy
			if 0 <= x2 < W and 0 <= y2 < H and board[x2][y2].pieces > 0:
				dfs(x2, y2)
	for x in range(W):
		for y in range(H):
			if board[x][y].dvonn: dfs(x, y)
	for x in range(W):
		for y in range(H):
			if board[x][y].pieces > 0 and not used[x][y]:
				board[x][y].clear()

def moves_for_player(board, player):
	'Returns a list of possible moves for the given player.'
	return [ ((x1,y1), (x2,y2)) for ((x1,y1), (x2,y2)) in all_moves(board)
	         if board[x1][y1].player == player ]

def encode(board, phase, player):
	'Returns the given game state encoded as a 50-character string.'
	val = 2*phase
	if player != NONE: val += player
	res = digits[val]
	for y in range(H):
		for x in range(W):
			if valid_field(x, y):
				f = board[x][y]
				if f.pieces == 0:
					res += digits[0]
				elif f.player < 0:
					assert f.dvonn and f.pieces == 1
					res += digits[1]
				else:
					assert f.pieces < 15
					res += digits[4*f.pieces + 2*f.dvonn + f.player - 2]
	return res

def decode(data):
	'Reverse of the above: returns the board, game phase and next player.'
	assert len(data) == 1 + N
	val = digits.index(data[0])
	phase = val//2
	assert phase <= COMPLETE
	player = val%2
	board = [ [ Field(x, y) for y in range(H) ] for x in range(W) ]
	pos = 1
	for y in range(H):
		for x in range(W):
			if valid_field(x, y):
				val = digits.index(data[pos])
				pos += 1
				if val == 1:
					board[x][y].pieces = 1
					board[x][y].dvonn  = True
				elif val > 1:
					board[x][y].pieces = (val + 2)//4
					board[x][y].dvonn  = bool((val + 2)//2%2)
					board[x][y].player = (val + 2)%2
	assert pos == N + 1
	return board, phase, player

def print_plain(board):
	'Prints the board in a simple, human-readable plain-text format.'

	for y in range(H):
		line = abs(y - H//2)*"  "
		for x in range(W):
			if valid_field(x, y):
				line += " " + str(board[x][y])
		print line

if __name__ == '__main__':
	initialize()

	op = optparse.OptionParser()
	op.add_option("--state", metavar="DESC", help="50-character state description to use")
	op.add_option("--transcript", metavar="PATH", help="path to transcript file to read")
	op.add_option("--logfile", metavar="PATH", help="path to log file to read")
	op.add_option("--truncate", metavar="N", help="truncate history to first N moves")
	op.add_option("--output", metavar="TYPE", help="type of output: state, plain, transcript, logfile, dvonner, text, json")
	(options, args) = op.parse_args()

	assert sum(getattr(options, x) is not None
		for x in ['state', 'transcript', 'logfile']) <= 1

	# Initialize game data. These fields should be kept consistent:
	board = [ [ Field(x, y) for y in range(H) ] for x in range(W) ]
	phase, player = PLACEMENT, WHITE
	metadata = {}                     # only for transcript
	moves, resigned = [], False       # only for log/transcript

	if options.state:
		board, phase, player = decode(options.state)
		moves = resigned = None
	if options.transcript or options.logfile:
		if options.transcript:
			metadata, moves = parse_transcript(file(options.transcript).read())
		else:
			metadata, moves = parse_logfile(file(options.logfile).read())

	if moves and options.truncate > 0:
		moves = moves[:int(options.truncate)]

	history = []
	# Update board state by executing moves:
	for i in range(len(moves or [])):
		if moves[i] is None:
			if moves_for_player(board, next_player(moves[:i])) != []:
				resigned = True
				break
		else:
			if i < N:
				x, y = moves[i]
				board[x][y].place(i)
			else:
				(x1, y1), (x2, y2) = moves[i]
				assert board[x1][y1].player == (i - N)%2
				board[x1][y1].move(board[x2][y2])
				remove_disconnected(board)
		if phase == MOVEMENT and all_moves(board) == []:
			phase = COMPLETE
			player = NONE
		else:
			phase = game_phase(moves[:i+1])
			player = next_player(moves[:i+1])
		history.append((clone_board(board), phase, player))

	if resigned: phase = COMPLETE
	if phase == COMPLETE: player = NONE

	for arg in args:
		assert player != NONE
		if len(arg) == 2:
			assert phase == PLACEMENT
			places = [ (x,y) for x in range(W) for y in range(H)
				if valid_field(x,y) and board[x][y].pieces == 0 ]
			x, y = fields_by_id[arg]
			assert (x,y) in places
			board[x][y].place(N - len(places))
			if moves != None: moves.append((x, y))
			if len(places) > 1:
				player = 1 - player
			else:
				phase = MOVEMENT
		elif arg == "PASS":
			assert phase == MOVEMENT
			assert moves_for_player(board, player) == []
			player = 1 - player
		elif len(arg) == 4:
			assert phase == MOVEMENT
			(x1, y1) = fields_by_id[arg[0:2]]
			(x2, y2) = fields_by_id[arg[2:4]]
			assert ((x1,y1),(x2,y2)) in moves_for_player(board, player)
			board[x1][y1].move(board[x2][y2])
			remove_disconnected(board)
			if moves != None: moves.append(((x1,y1),(x2,y2)))
			player = 1 - player
		else:
			assert not "Invalid move given as argument!"
		if phase == MOVEMENT and all_moves(board) == []: phase = COMPLETE
		if phase == COMPLETE: player = NONE

	if options.output is None or options.output == 'state':
		print encode(board, phase, player)
	elif options.output == 'plain':
		print_plain(board)
	elif options.output == 'transcript':
		print format_transcript(metadata, moves, phase == COMPLETE)
	elif options.output == 'logfile':
		sys.stdout.write(format_logfile({}, moves))
	elif options.output == 'dvonner':
		sys.stdout.write(format_dvonner({}, moves))
	elif options.output == 'json':
		parts = []
		for move, (board, phase, player) in zip(moves, history):
			parts.append('{' +
				'"move":"' + format_move(move) + '"' +
				',' +
				'"state":"' + encode(board, phase, player) + '"' +
				'}')
		sys.stdout.write(('[' + ','.join(parts) + ']'))
	else:
		assert not "Unknown output format!"

