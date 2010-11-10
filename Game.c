#include <assert.h>
#include <string.h>
#include "Game.h"

#ifdef ZOBRIST
/* Include the zobrist key tables directly into the source here, because they
   are not needed anywhere else, and this allows compiling without the key file
   if ZOBRIST is not defined: */
#include "Game-zobrist-keys.c"
#endif

Move move_null = {  0,  0,  0,  0 };
Move move_pass = { -1, -1, -1, -1 };

const int DR[6] = {  0, -1, -1,  0, +1, +1 };
const int DC[6] = { +1,  0, -1, -1,  0, +1 };

static int max(int i, int j) { return i > j ? i : j; }

EXTERN int distance(int r1, int c1, int r2, int c2)
{
	int dx = c2 - c1;
	int dy = r2 - r1;
	int dz = dx - dy;
	return max(max(abs(dx), abs(dy)), abs(dz));
}

EXTERN void board_clear(Board *board)
{
	int r, c;
	board->moves = 0;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			board->fields[r][c].player = NONE;
			board->fields[r][c].pieces = 0;
			board->fields[r][c].dvonns = 0;
			board->fields[r][c].removed =
				distance(r, c, H/2, W/2) <= W/2 ? 0 : -1;
			board->fields[r][c].mobile = 6;
		}
	}
#ifdef ZOBRIST
	board->hash = init_key;
#endif
}

#ifdef ZOBRIST
/* Encodes the given field's `pieces', `player' and `dvonns' field into a
   unique number between 0 and and 4*(N + 1) == 200, exclusive. */
static int field_id(const struct Field *f) {
	int res = 4*f->pieces + 2*f->player;
	if (f->dvonns) ++res;
	return res;
}

/* Recalculates the Zobrist hash of a board. */
EXTERN hash_t zobrist_hash(const Board *board) {
	const Field *f;
	int n;
	hash_t hash;

	hash = init_key;
	if (board->moves % 2) hash ^= player_key;
	if (board->moves >= N) hash ^= phase_key;
	for (n = 0; n < H*W; ++n) {
		f = &board->fields[0][n];
		if (!f->removed && f->pieces) hash ^= field_key[field_id(f)][0][n];
	}
	return hash;
}
#endif

static void place(Board *board, const Move *m)
{
	Field *f = &board->fields[m->r1][m->c1];
	f->pieces = 1;
	if (board->moves < D) {
		f->dvonns = 1;
	} else {
		f->player = board->moves & 1;
	}
#ifdef ZOBRIST
	board->hash ^= field_key[field_id(f)][m->r1][m->c1];
#endif
}

static void unplace(Board *board, const Move *m)
{
	Field *f = &board->fields[m->r1][m->c1];
#ifdef ZOBRIST
	board->hash ^= field_key[field_id(f)][m->r1][m->c1];
#endif
	f->player = NONE;
	f->pieces = 0;
	f->dvonns = 0;
}

/* Temporary space shared by mark_reachable() and remove_reachable(): */
static bool reachable[H][W];

static void mark_reachable(Board *board, int n) {
	const int *step;
	int m;

	reachable[0][n] = true;
	for (step = board_steps[1][0][n]; *step; ++step) {
		m = n + *step;
		if (!board->fields[0][m].removed && !reachable[0][m]) {
			mark_reachable(board, m);
		}
	}
}

static void remove_unreachable(Board *board)
{
	int n;
	Field *f;

	memset(reachable, 0, sizeof(reachable));
	for (n = 0; n < H*W; ++n) {
		f = &board->fields[0][n];
		if (!f->removed && f->dvonns && !reachable[0][n]) {
			mark_reachable(board, n);
		}
	}
	for (n = 0; n < H*W; ++n) {
		f = &board->fields[0][n];
		if (!f->removed && !reachable[0][n]) {
			f->removed = board->moves;
#ifdef ZOBRIST
			board->hash ^= field_key[field_id(f)][0][n];
#endif
		}
	}
}

/* Returns whether the field at (r,c) could be a bridge (a field that connects
   two or more otherwise unconnected segments of the board) by analyzing its
   neighbours: a field cannot be a bridge if all of its neighbours are adjacent
   to each other. */
static bool may_be_bridge(Board *board, int r, int c)
{
	static bool bridge_index[1<<6] = {
		0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0,
		0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0,
		0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0,
		0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0 };

	int d, mask = 0;

	for (d = 0; d < 6; ++d) {
		int nr = r + DR[d], nc = c + DC[d];
		if (nr >= 0 && nr < H && nc >= 0 && nc < W &&
			!board->fields[nr][nc].removed) mask |= 1<<d;
	}
	return bridge_index[mask];
}

static void stack(Board *board, const Move *m)
{
	Color tmp_player;
	Field *f = &board->fields[m->r1][m->c1];
	Field *g = &board->fields[m->r2][m->c2];

#ifdef ZOBRIST
	board->hash ^= field_key[field_id(f)][m->r1][m->c1];
	board->hash ^= field_key[field_id(g)][m->r2][m->c2];
#endif
	tmp_player = f->player;
	f->player = g->player;
	g->player = tmp_player;
	g->pieces += f->pieces;
	g->dvonns += f->dvonns;
	f->removed = board->moves;
#ifdef ZOBRIST
	board->hash ^= field_key[field_id(g)][m->r2][m->c2];
#endif
	/* We must remove disconnected fields, but since remove_unreachable()
		is expensive, we try to elide it if permissible: */
	if (f->dvonns || may_be_bridge(board, m->r1, m->c1)) {
		remove_unreachable(board);
	}
}

static void restore_unreachable(Board *board, int n)
{
	int m;
	const int *step;

#ifdef ZOBRIST
	board->hash ^= field_key[field_id(&board->fields[0][n])][0][n];
#endif
	board->fields[0][n].removed = 0;
	for (step = board_steps[1][0][n]; *step; ++step) {
		m = n + *step;
		if (board->fields[0][m].removed == board->moves) {
			restore_unreachable(board, m);
		}
	}
}

static void unstack(Board *board, const Move *m)
{
	Color tmp_player;
	Field *f = &board->fields[m->r1][m->c1];
	Field *g = &board->fields[m->r2][m->c2];

	/* Note to self: do not reorder anything here, or you will break things!
	   Updating the Zobrist hash is sensitive to the order of these statements,
	   because the destination field itself may have been removed, and must be
	   restored by restore_unreachable() with the correct contents. */

#ifdef ZOBRIST
	board->hash ^= field_key[field_id(g)][m->r2][m->c2];
#endif
	tmp_player = f->player;
	f->player = g->player;
	assert(f->removed == board->moves);
	restore_unreachable(board, W*m->r1 + m->c1);
	g->player = tmp_player;
	g->pieces -= f->pieces;
	g->dvonns -= f->dvonns;
#ifdef ZOBRIST
	board->hash ^= field_key[field_id(g)][m->r2][m->c2];
#endif
}

/* Used also by IO.c: */
EXTERN void update_neighbour_mobility(Board *board, int r1, int c1, int diff)
{
	const int *step;
	for (step = board_steps[1][r1][c1]; *step; ++step) {
		(&board->fields[r1][c1] + *step)->mobile += diff;
	}
}

EXTERN void board_do(Board *board, const Move *m)
{
	if (m->r2 >= 0) {
		stack(board, m);
		update_neighbour_mobility(board, m->r1, m->c1, +1);
	} else if (m->r1 >= 0) {
		place(board, m);
		update_neighbour_mobility(board, m->r1, m->c1, -1);
	}
	++board->moves;
#ifdef ZOBRIST
	if (board->moves == N) board->hash ^= phase_key;
	board->hash ^= player_key;
#endif
}

EXTERN void board_undo(Board *board, const Move *m)
{
#ifdef ZOBRIST
	board->hash ^= player_key;
	if (board->moves == N) board->hash ^= phase_key;
#endif
	--board->moves;
	if (m->r2 >= 0) {  /* stack */
		unstack(board, m);
		update_neighbour_mobility(board, m->r1, m->c1, -1);
	} else if (m->r1 >= 0) {  /* place */
		unplace(board, m);
		update_neighbour_mobility(board, m->r1, m->c1, +1);
	}
}

static void validate_mobility(const Board *board, int r1, int c1)
{
	int d, r2, c2, n = board->fields[r1][c1].mobile;

	assert(n >= 0 && n <= 6);
	for (d = 0; d < 6; ++d) {
		r2 = r1 + DR[d];
		if (r2 >= 0 && r2 < H) {
			c2 = c1 + DC[d];
			if (c2 >= 0 && c2 < W) {
				if (board->moves < N) {
					if (board->fields[r2][c2].pieces > 0) ++n;
				} else {
					if (!board->fields[r2][c2].removed) ++n;
				}
			}
		}
	}
	assert(n == 6);
}

EXTERN void board_validate(const Board *board)
{
	int r, c;

	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			const Field *f = &board->fields[r][c];
			assert(f->pieces <= N);
			assert(f->dvonns <= D);
			assert(f->dvonns <= f->pieces);
			if (f->dvonns == f->pieces || f->pieces == 0) {
				assert(f->player == NONE);
			}
			if (f->removed < 0) continue;
			if (board->moves < N) {
				assert(f->pieces == 0 || f->pieces == 1);
				assert(f->removed == 0);
			} else {
				assert(f->pieces > 0);
				assert(f->removed < board->moves);
			}
			if (!f->removed) validate_mobility(board, r, c);
		}
	}
#ifdef ZOBRIST
	assert(zobrist_hash(board) == board->hash);
#endif
	/* This doesn't really belong here, but I want to check it somewhere: */
	assert(sizeof(Move) == sizeof(int));
}

/* Generates a list of possible setup moves. */
static int gen_places(const Board *board, Move moves[N])
{
	int r, c, nmove = 0;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			if (!board->fields[r][c].pieces && !board->fields[r][c].removed) {
				Move new_move = { r, c, -1, -1 };
				moves[nmove] = new_move;
				++nmove;
			}
		}
	}
	return nmove;
}

/* Generates a list of possible stacking moves, either for one of the two
   players (if `player' is WHITE or BLACK) or both (if `player' is NONE): */
static int gen_stacks(const Board *board, Move *moves, Color player)
{
	const Field *f, *g;
	const int *step;
	int n, m, nmove = 0;

	for (n = 0; n < H*W; ++n) {
		f = &board->fields[0][n];
		if (!f->removed && f->mobile && f->player >= 0 &&
			(player == NONE || f->player == player)) {
			for (step = board_steps[f->pieces][0][n]; *step; ++step) {
				m = n + *step;
				g = f + *step;
				if (!g->removed) {
					Move new_move = { n/W, n%W, m/W, m%W};
					moves[nmove] = new_move;
					++nmove;
				}
			}
		}
	}
	return nmove;
}

EXTERN int generate_all_moves(const Board *board, Move moves[2*M])
{
	static Move dummy_moves[2*M];

	if (!moves) moves = dummy_moves;

	if (board->moves < N) {
		return gen_places(board, moves);
	} else {
		return gen_stacks(board, moves, NONE);
	}
}

EXTERN int generate_moves(const Board *board, Move moves[M])
{
	static Move dummy_moves[M];

	if (!moves) moves = dummy_moves;

	if (board->moves < N) {
		return gen_places(board, moves);
	} else {
		int nmove;

		nmove = gen_stacks(board, moves, next_player(board));
		if (nmove == 0) moves[nmove++] = move_pass;
		return nmove;
	}
}

EXTERN void board_scores(const Board *board, int scores[2])
{
	int n;
	const Field *f;

	scores[0] = scores[1] = 0;
	for (n = 0; n < H*W; ++n) {
		f = &board->fields[0][n];
		if (!f->removed && (f->player == 0 || f->player == 1)) {
			scores[f->player] += f->pieces;
		}
	}
}

EXTERN bool valid_move(const Board *board, const Move *move)
{
	Move moves[M];
	int i, nmove;

	nmove = generate_moves(board, moves);
	for (i = 0; i < nmove; ++i) {
		if (move_compare(&moves[i], move) == 0) return true;
	}
	return false;
}

EXTERN int board_score(const Board *board)
{
	int sc[2], player;

	player = next_player(board);
	board_scores(board, sc);
	return sc[player] - sc[1 - player];
}
