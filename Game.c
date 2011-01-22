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
	const Field *f = &board->fields[n];
	unsigned key = (4*f->pieces + 2*f->player + (bool)f->dvonns)*N + n;
	/* Key is always positive here, which is why this works: */
	board->hash.ui[0] ^= zobrist_field_key[key];
	board->hash.ui[1] ^= zobrist_field_key[key - 1];
}

/* Recalculates the Zobrist hash of a board. */
LargeInteger zobrist_hash(const Board *board_in) {
	const Field *f;
	int n;
	Board temp;

	temp = *board_in;
	zobrist_init(&temp);
	if (next_player(&temp) == BLACK) zobrist_toggle_player(&temp);
	if (temp.moves >= N) zobrist_toggle_phase(&temp);
	for (n = 0; n < N; ++n) {
		f = &temp.fields[n];
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

Move move_null = {  0,  0 };
Move move_pass = { -1, -1 };

void board_clear(Board *board)
{
	Field *f;
	board->moves = 0;
	for (f = board->fields; f != board->fields + N; ++f) {
		f->player  = NONE;
		f->pieces  = 0;
		f->dvonns  = 0;
		f->removed = 0;
		f->mobile  = 6;
	}
	board->dvonns = 0;
	zobrist_init(board);
}

static void place(Board *board, const Move *m)
{
	int n = m->src;
	Field *f = &board->fields[n];
	f->pieces = 1;
	if (board->moves < D) {
		f->dvonns = 1;
		board->dvonns |= (1LL<<n);
	} else {
		f->player = board->moves & 1;
	}
	zobrist_toggle_field(board, n);
}

static void unplace(Board *board, const Move *m)
{
	int n = m->src;
	Field *f = &board->fields[n];
	if (f->dvonns) board->dvonns &= ~(1LL<<n);
	zobrist_toggle_field(board, n);
	f->player = NONE;
	f->pieces = 0;
	f->dvonns = 0;
}

/* Temporary space shared by mark_reachable() and remove_reachable(): */
static bool reachable[N];

static void mark_reachable(Board *board, int n) {
	const int *step;
	int m;

	reachable[n] = true;
	for (step = board_steps[1][n]; *step; ++step) {
		m = n + *step;
		if (!board->fields[m].removed && !reachable[m]) {
			mark_reachable(board, m);
		}
	}
}

static void remove_unreachable(Board *board)
{
	int n;
	Field *f;

	memset(reachable, 0, sizeof(reachable));
	for (n = 0; n < N; ++n) {
		f = &board->fields[n];
		if (!f->removed && f->dvonns && !reachable[n]) {
			mark_reachable(board, n);
		}
	}
	for (n = 0; n < N; ++n) {
		f = &board->fields[n];
		if (!f->removed && !reachable[n]) {
			f->removed = board->moves;
			zobrist_toggle_field(board, n);
		}
	}
}

/* Returns whether the field at (r,c) could be a bridge (a field that connects
   two or more otherwise unconnected segments of the board) by analyzing its
   neighbours: a field cannot be a bridge if all of its neighbours are adjacent
   to each other. */
static bool may_be_bridge(Board *board, int n)
{
	static bool bridge_index[1<<6] = {
		0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0,
		0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0,
		0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0,
		0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0 };

	const Field *f = &board->fields[n];

	if (f->mobile < 2 || f->mobile > 4) return false;

	const int *step;
	int mask = 0;
	int used = board_neighbours[n];
	for (step = board_steps[1][n]; *step; ++step) {
		int bit = used & ~(used - 1);
		used ^= bit;
		if (!f[*step].removed) mask |= bit;
	}
	return bridge_index[mask];
}

static void stack(Board *board, const Move *m)
{
	Color tmp_player;
	int n1 = m->src, n2 = m->dst;
	Field *f = &board->fields[n1];
	Field *g = &board->fields[n2];

	zobrist_toggle_field(board, n1);
	zobrist_toggle_field(board, n2);
	if (f->dvonns) {
		board->dvonns &= ~(1LL<<n1);
		if (!g->dvonns) board->dvonns |= (1LL<<n2);
	}
	tmp_player = f->player;
	f->player = g->player;
	g->player = tmp_player;
	g->pieces += f->pieces;
	g->dvonns += f->dvonns;
	f->removed = board->moves;
	zobrist_toggle_field(board, n2);

	/* We must remove disconnected fields, but since remove_unreachable()
		is expensive, we try to elide it if permissible: */
	if (f->dvonns || may_be_bridge(board, n1)) {
		remove_unreachable(board);
	}
}

static void restore_unreachable(Board *board, int n)
{
	int m;
	const int *step;

	zobrist_toggle_field(board, n);
	board->fields[n].removed = 0;
	for (step = board_steps[1][n]; *step; ++step) {
		m = n + *step;
		if (board->fields[m].removed == board->moves) {
			restore_unreachable(board, m);
		}
	}
}

static void unstack(Board *board, const Move *m)
{
	Color tmp_player;
	int n1 = m->src, n2 = m->dst;
	Field *f = &board->fields[n1];
	Field *g = &board->fields[n2];

	/* Note to self: do not reorder anything here, or you will break things!
	   Updating the Zobrist hash is sensitive to the order of these statements,
	   because the destination field itself may have been removed, and must be
	   restored by restore_unreachable() with the correct contents. */

	zobrist_toggle_field(board, n2);
	tmp_player = f->player;
	f->player = g->player;
	assert(f->removed == board->moves);
	restore_unreachable(board, n1);
	g->player = tmp_player;
	g->pieces -= f->pieces;
	g->dvonns -= f->dvonns;
	zobrist_toggle_field(board, n2);
	if (f->dvonns) {
		board->dvonns |= (1LL<<n1);
		if (!g->dvonns) board->dvonns &= ~(1LL<<n2);
	}
}

/* Used also by IO.c: */
void update_neighbour_mobility(Board *board, int n, int diff)
{
	const int *step;

	for (step = board_steps[1][n]; *step; ++step) {
		board->fields[n + *step].mobile += diff;
	}
}

void board_do(Board *board, const Move *m)
{
	if (m->dst >= 0) {  /* stack */
		stack(board, m);
		update_neighbour_mobility(board, m->src, +1);
	} else if (m->src >= 0) {  /* place */
		place(board, m);
		update_neighbour_mobility(board, m->src, -1);
	}
	++board->moves;
	if (board->moves == N) {
		zobrist_toggle_phase(board);
	} else {
		zobrist_toggle_player(board);
	}
}

void board_undo(Board *board, const Move *m)
{
	if (board->moves == N) {
		zobrist_toggle_phase(board);
	} else {
		zobrist_toggle_player(board);
	}
	--board->moves;
	if (m->dst >= 0) {  /* stack */
		unstack(board, m);
		update_neighbour_mobility(board, m->src, -1);
	} else if (m->src >= 0) {  /* place */
		unplace(board, m);
		update_neighbour_mobility(board, m->src, +1);
	}
}

#ifndef NDEBUG
static void validate_mobility(const Board *board, int n)
{
	int mob = board->fields[n].mobile;
	const int *step;

	assert(mob >= 0 && mob <= 6);
	for (step = board_steps[1][n]; *step; ++step) {
		if (board->moves < N) {
			if (board->fields[n + *step].pieces > 0) ++mob;
		} else {
			if (!board->fields[n + *step].removed) ++mob;
		}
	}
	assert(mob == 6);
}

void board_validate(const Board *board)
{
	int n;
	long long dvonns = 0;

	for (n = 0; n < N; ++n) {
		const Field *f = &board->fields[n];
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
		if (!f->removed) validate_mobility(board, n);
		if (!f->removed && f->dvonns) dvonns |= (1LL<<n);
	}
#ifdef ZOBRIST
	assert(zobrist_hash(board).ull == board->hash.ull);
#endif
	assert(dvonns == board->dvonns);

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
	int n, nmove = 0;
	for (n = 0; n < N; ++n) {
		if (!board->fields[n].pieces) {
			Move new_move = { n, -1 };
			moves[nmove++] = new_move;
		}
	}
	return nmove;
}

/* Generates a list of possible stacking moves, either for one of the two
   players (if `player' is WHITE or BLACK) or both (if `player' is NONE): */
static int gen_stacks(const Board *board, Move *moves, Color player)
{
	const Field *f;
	const int *step;
	int n, nmove = 0;

	for (n = 0; n < N; ++n) {
		f = &board->fields[n];
		if (!f->removed && f->mobile && f->player == player)
		{
			for (step = board_steps[f->pieces][n]; *step; ++step) {
				if (!f[*step].removed) {
					Move new_move = { n, n + *step };
					moves[nmove++] = new_move;
				}
			}
		}
	}
	return nmove;
}

int generate_all_moves(const Board *board, Move moves[2*M])
{
	static Move dummy_moves[2*M];

	if (!moves) moves = dummy_moves;

	if (board->moves < N) {
		return gen_places(board, moves);
	} else {
		/* This makes two passes over the board, which is less than optimal!
		   Fortunately, generate_all_moves() is not performance-critical. */
		int n = gen_stacks(board, moves, WHITE);
		return n + gen_stacks(board, moves + n, BLACK);
	}
}

int generate_moves(const Board *board, Move moves[M])
{
	static Move dummy_moves[M];

	if (!moves) moves = dummy_moves;

	if (board->moves < N) {
		return gen_places(board, moves);
	} else {
		int nmove = gen_stacks(board, moves, next_player(board));
		if (nmove == 0) moves[nmove++] = move_pass;
		return nmove;
	}
}

bool valid_move(const Board *board, const Move *move)
{
	Move moves[M];
	int i, nmove;

	nmove = generate_moves(board, moves);
	for (i = 0; i < nmove; ++i) {
		if (move_compare(&moves[i], move) == 0) return true;
	}
	return false;
}

void board_scores(const Board *board, int scores[2])
{
	int n;
	const Field *f;

	scores[0] = scores[1] = 0;
	for (n = 0; n < N; ++n) {
		f = &board->fields[n];
		if (!f->removed && f->player >= 0) scores[f->player] += f->pieces;
	}
}

int board_score(const Board *board)
{
	int sc[2], player;

	player = next_player(board);
	board_scores(board, sc);
	return sc[player] - sc[1 - player];
}
