#include "AI.h"
#include "IO.h"
#include "Time.h"
#include "TT.h"
#include "MO.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Maximum search depth: */
static const int max_depth = 20;

/* Search algorithm parameters: */
bool ai_use_tt     = true;
bool ai_use_mo     = true;
bool ai_use_killer = true;

/* Global flag to abort search: */
static volatile bool aborted = false;
static unsigned int rng_seed = 0;

static void reset_rng()
{
	while (rng_seed == 0) rng_seed = rand();
	srand(rng_seed);
}

/* Implements depth-first minimax search with alpha-beta pruning.

   Takes the current game state in `board' and `pass' (the number of passes
   in succession) and the maximum search depth `depth', returns the value of
   the game position in the range [lo+1:hi-1], or if the value is <= lo, then
   it returns an upper bound on the game value, or if the value is >= hi, then
   it returns a lower bound on the value.

   If `best' is not NULL, then the best move is assigned to *best.

   Note that we try to return as tight an upper bound as possible without
   searching more nodes than stricly necessary. This means in particular that
   we try to return bounds which are unequal to lo and hi if possible. This
   is beneficial when re-using transposition table entries.

   Note that the search may be aborted by setting the global variable `aborted'
   to `true'. In that case, dfs() returns 0, and the caller (which includes
   dfs() itself) must ensure that the value is not used as a valid result! This
   means that all calls to dfs() should be followed by checking `aborted' before
   using the return value.
*/
static val_t dfs(Board *board, int depth, int pass, int lo, int hi, Move *best)
{
	hash_t hash = (hash_t)-1;
#ifdef TT_DEBUG
	unsigned char data[50];
#endif
	TTEntry *entry = NULL;
	val_t res = min_val;
	Move killer = move_null;

	if (ai_use_tt) { /* look up in transposition table: */
#ifdef TT_DEBUG
		serialize_board(board, data);
		hash = fnv1(data, 50);
#else
		hash = hash_board(board);
#endif
		entry = &tt[hash%tt_size];
		if (entry->hash == hash) {
#ifdef TT_DEBUG  /* detect hash collisions */
			assert(memcmp(entry->data, data, 50) == 0);
#endif
			if (entry->depth >= depth && 
				(best == NULL || valid_move(board, &entry->killer))) {
				if (best) *best = entry->killer;
				if (entry->lo == entry->hi) return entry->lo;
				if (entry->lo >= hi) return entry->lo;
				if (entry->hi <= lo) return entry->hi;
				res = entry->lo;
			}
			if (ai_use_killer) killer = entry->killer;
		}
	}
	if (pass == 2) {  /* evaluate end position */
		assert(best == NULL);
		res = eval_end(board);
		if (ai_use_tt) {
			entry->hash   = hash;
			entry->lo     = res;
			entry->hi     = res;
			entry->depth  = max_depth;
			entry->killer = move_null;
#ifdef TT_DEBUG
			memcpy(entry->data, data, 50);
#endif
		}
	} else if (depth == 0) {  /* evaluate intermediate position */
		assert(best == NULL);
		res = eval_intermediate(board);
		if (ai_use_tt) {
			/* FIXME: eval_intermediate() could end up calling eval_end() if 
			   the current position is actually an end position. In that case,
			   we should really store depth = max_depth here! */
			entry->hash  = hash;
			entry->lo    = res;
			entry->hi    = res;
			entry->depth = 0;
			entry->killer = move_null;
#ifdef TT_DEBUG
			memcpy(entry->data, data, 50);
#endif
		}
	} else {  /* evaluate interior node */
		Move moves[M];
		int n, nmove = generate_moves(board, moves);
		int next_lo = -hi;
		int next_hi = (-res < -lo) ? -res : -lo;

		if (best) *best = move_null;  /* for integrity checking */
		if (nmove == 1) {
			/* Only one move available: */
			if (best != NULL) *best = moves[0];
			board_do(board, &moves[0]);
			res = -dfs(board, depth, (move_passes(&moves[0]) ? pass+1 : 0),
					   next_lo, next_hi, NULL);
			board_undo(board, &moves[0]);
			if (aborted) return 0;
		} else {
			/* Multiple moves available */
			assert(nmove > 1);

			/* FIXME: maybe shuffle whenever depth >= 2 too,
			   to get less variation in search times? */
			if (best != NULL) {
				reset_rng();
				shuffle_moves(moves, nmove);
			}

			/* Move ordering: */
			if (ai_use_mo) order_moves(board, moves, nmove);

			/* Killer heuristic: */
			if (!move_is_null(&killer)) move_to_front(moves, nmove, killer);

			for (n = 0; n < nmove; ++n) {
				val_t val;

				/* Evaluate position after n'th move: */
				board_do(board, &moves[n]);
				val = -dfs(board, depth - 1, 0, next_lo, next_hi, NULL);
				board_undo(board, &moves[n]);
				if (aborted) return 0;

				/* Update value bounds: */
				if (val > res) {
					res = val;
					if (-res < next_hi) next_hi = -res;
					killer = moves[n];
				}
				if (res >= hi) break;
			}
		}
		if (best) *best = killer;
		assert(best == NULL || !move_is_null(best));
		if (ai_use_tt) {  /* FIXME: replacement policy? */
			entry->hash  = hash;
			entry->lo    = res > lo ? res : min_val;
			entry->hi    = res < hi ? res : max_val;
			entry->depth = depth;
			entry->killer = killer;
#ifdef TT_DEBUG
			memcpy(entry->data, data, 50);
#endif
		}
	}
	return res;
}

static void set_aborted(void *arg)
{
	(void)arg;  /* UNUSED */

	aborted = true;
}

static int est_moves_left(const Board *board, Color player)
{
	int r1, c1, r2, c2, d, res = 0;
	const Field *f;

	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			f = &board->fields[r1][c1];
			if (!f->removed && f->player == player) {
				for (d = 0; d < 6; ++d) {
					r2 = r1 + DR[d]*f->pieces;
					if (r2 >= 0 && r2 < H) {
						c2 = c1 + DC[d]*f->pieces;
						if (c2 >= 0 && c2 < W) {
							if (!board->fields[r2][c2].removed) {
								++res;
								break;
							}
						}
					}
				}
			}
		}
	}
	return res;
}

EXTERN bool ai_select_move(Board *board, Move *move)
{
	val_t val;
	Move moves[M];
	int nmove = generate_moves(board, moves);
	bool alarm_set = false;

	/* Check if we have any moves to make: */
	if (nmove == 0) {
		fprintf(stderr, "no moves available!\n");
		return false;
	}
	if (nmove == 1) {
		fprintf(stderr, "one move available!\n");
		*move = moves[0];
		return true;
	}

	if (board->moves < N) {
		/* Fixed-depth search during placement phase: */
		val = dfs(board, 2, 0, min_val, max_val, move);
		fprintf(stderr, "v:%d\n", val);
	} else {
		/* Increment max depth during placement phase: */
		static int depth = 2;
		int moves_left;
		double start = time_used(), left = time_left();
		double used = 0, budget = 0;

		fprintf(stderr, "%.3fs\n", start);

		/* Estimate number of moves left: */
		moves_left = est_moves_left(board, next_player(board));
		if (moves_left > 12) moves_left = 12;
		if (moves_left <  3) moves_left =  3;
		budget = left/moves_left;
		fprintf(stderr, "budget: %.3fs for %d moves\n", budget, est_moves_left(board, next_player(board)));

		/* Killer heuristic is most effective when the transposition table
		   contains the information from one ply ago, instead of two plies: */
		if (ai_use_killer) --depth;

		eval_count_reset();
		aborted = false;
		for (;;) {
			/* DFS for best value and move: */
			Move new_move;
			val_t new_val = dfs(board, depth, 0, min_val, max_val, &new_move);
			used = time_used() - start;

			if (aborted) {
				fprintf(stderr, "WARNING: aborted after %.3fs!\n", used);
				--depth;
				break;
			}

			/* Update new best value and move: */
			val = new_val;
			*move = new_move;

			/* Report time used: */
			fprintf(stderr, "d:%d v:%d e:%d m:%s u:%.3fs\n",
				depth, val, eval_count_total(), format_move(move), used);

			if (used > budget) {
				fprintf(stderr, "WARNING: over budget!\n");
				--depth;
				break;
			}

			/* Determine whether to do another pass at increased search depth: */
			if (depth == max_depth) break;
			if (used > ((depth%2 == 0) ? 0.1 : 0.4)*budget) break;
			++depth;

			/* Schedule alarm to abort search instead of going over budget: */
			if (!alarm_set) {
				set_alarm(budget - used, set_aborted, NULL);
				alarm_set = true;
			}
		}
		if (alarm_set) {
			clear_alarm();
			aborted = false;
		}
	}
	return true;
}

EXTERN bool ai_select_move_fixed(Board *board, Move *move, int depth)
{
	Move moves[M];
	int nmove = generate_moves(board, moves);
	val_t val;

	if (nmove == 0) {
		fprintf(stderr, "no moves available!\n");
		return false;
	}
	if (nmove == 1) {
		fprintf(stderr, "one move available!\n");
		*move = moves[0];
		return true;
	}
	eval_count_reset();
	val = dfs(board, depth, 0, min_val, max_val, move);
	fprintf(stderr, "depth=%d val=%d evaluated: %d\n",
		depth, val, eval_count_total());
	return true;
}

EXTERN val_t ai_evaluate(const Board *board)
{
	return eval_intermediate(board);
}

EXTERN int ai_extract_pv(Board *board, Move *moves, int nmove)
{
	int n;
	hash_t hash;
	TTEntry *entry;

	for (n = 0; n < nmove; ++n)
	{
		hash = hash_board(board);
		entry = &tt[hash%tt_size];
		if (entry->hash != hash || move_is_null(&entry->killer)) break;
		assert(valid_move(board, &entry->killer));
		moves[n] = entry->killer;
		board_do(board, &moves[n]);
	}
	nmove = n;
	while (n > 0) board_undo(board, &moves[--n]);
	return nmove;
}
