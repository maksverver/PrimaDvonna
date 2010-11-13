#include <assert.h>
#include <string.h>
#include "Game.h"

#ifdef ZOBRIST
/* Include the zobrist key tables directly into the source here, because they
   are not needed anywhere else, and this allows compiling without the key file
   if ZOBRIST is not defined: */
#include "Game-zobrist-keys.c"

/* Initializes the hash for an empty board. */
static void zobrist_init(Board *board)
{
	board->hash.ull = zobrist_init_key;
}

/* Updates the hash to reflect changing the next player to move. */
static void zobrist_toggle_player(Board *board)
{
	board->hash.ull ^= zobrist_player_key;
}

/* Updates the hash to reflect changing the game phase. */
static void zobrist_toggle_phase(Board *board)
{
	board->hash.ull ^= zobrist_phase_key;
}

/* Updates the hash to reflect adding/removing the contents of the n-th field,
   which must currently hold a valid stack (i.e. pieces > 0). Contents consist
   of pieces, player and dvonns (zero or not); note that the `removed' field
   is exluded, so this function can safely be used to toggle fields after
   they have been removed. */
static void zobrist_toggle_field(Board *board, int n)
{
	const Field *f = &board->fields[0][n];
	unsigned key = (4*f->pieces + 2*f->player + (bool)f->dvonns)*H*W + n;
	/* Key is always positive here, which is why this works: */
	board->hash.ui[0] ^= zobrist_field_key[key];
	board->hash.ui[1] ^= zobrist_field_key[key - 1];
}

/* Recalculates the Zobrist hash of a board. */
EXTERN LargeInteger zobrist_hash(const Board *board_in) {
	const Field *f;
	int n;
	Board temp;

	temp = *board_in;
	zobrist_init(&temp);
	if (next_player(&temp) == BLACK) zobrist_toggle_player(&temp);
	if (temp.moves >= N) zobrist_toggle_phase(&temp);
	for (n = 0; n < H*W; ++n) {
		f = &temp.fields[0][n];
		if (!f->removed && f->pieces) zobrist_toggle_field(&temp, n);
	}
	return temp.hash;
}

#else /* ndef ZOBRIST */
#define zobrist_init(board)
#define zobrist_toggle_phase(board)
#define zobrist_toggle_player(board)
#define zobrist_toggle_field(board, n)
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
	zobrist_init(board);
}

static void place(Board *board, const Move *m)
{
	int n = W*m->r1 + m->c1;
	Field *f = &board->fields[0][n];
	f->pieces = 1;
	if (board->moves < D) {
		f->dvonns = 1;
	} else {
		f->player = board->moves & 1;
	}
	zobrist_toggle_field(board, n);
}

static void unplace(Board *board, const Move *m)
{
	int n = W*m->r1 + m->c1;
	Field *f = &board->fields[0][n];
	zobrist_toggle_field(board, n);
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
			zobrist_toggle_field(board, n);
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
	int n1 = W*m->r1 + m->c1, n2 = W*m->r2 + m->c2;
	Field *f = &board->fields[0][n1];
	Field *g = &board->fields[0][n2];

	zobrist_toggle_field(board, n1);
	zobrist_toggle_field(board, n2);

	tmp_player = f->player;
	f->player = g->player;
	g->player = tmp_player;
	g->pieces += f->pieces;
	g->dvonns += f->dvonns;
	f->removed = board->moves;
	zobrist_toggle_field(board, n2);

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

	zobrist_toggle_field(board, n);
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
	int n1 = W*m->r1 + m->c1, n2 = W*m->r2 + m->c2;
	Field *f = &board->fields[0][n1];
	Field *g = &board->fields[0][n2];

	/* Note to self: do not reorder anything here, or you will break things!
	   Updating the Zobrist hash is sensitive to the order of these statements,
	   because the destination field itself may have been removed, and must be
	   restored by restore_unreachable() with the correct contents. */

	zobrist_toggle_field(board, n2);
	tmp_player = f->player;
	f->player = g->player;
	assert(f->removed == board->moves);
	restore_unreachable(board, W*m->r1 + m->c1);
	g->player = tmp_player;
	g->pieces -= f->pieces;
	g->dvonns -= f->dvonns;
	zobrist_toggle_field(board, n2);
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
	if (board->moves == N) {
		zobrist_toggle_phase(board);
	} else {
		zobrist_toggle_player(board);
	}
}

EXTERN void board_undo(Board *board, const Move *m)
{
	if (board->moves == N) {
		zobrist_toggle_phase(board);
	}
	else {
		zobrist_toggle_player(board);
	}
	--board->moves;
	if (m->r2 >= 0) {  /* stack */
		unstack(board, m);
		update_neighbour_mobility(board, m->r1, m->c1, -1);
	} else if (m->r1 >= 0) {  /* place */
		unplace(board, m);
		update_neighbour_mobility(board, m->r1, m->c1, +1);
	}
}

#ifndef NDEBUG
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
	assert(zobrist_hash(board).ull == board->hash.ull);
#endif

	/* Size checks don't really belong here, but I need to check somewhere: */
	assert(sizeof(Move) == sizeof(int));
#ifdef ZOBRIST
	assert(sizeof(board->hash.ui) == sizeof(board->hash.ull));
#endif
}
#endif /* NDEBUG */

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
