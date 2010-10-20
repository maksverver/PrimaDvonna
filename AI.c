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
volatile bool aborted = false;

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
	Move killer = move_pass;

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
			entry->hash  = hash;
			entry->lo    = res;
			entry->hi    = res;
			entry->depth = max_depth;
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
#ifdef TT_DEBUG
			memcpy(entry->data, data, 50);
#endif
		}
	} else {  /* evaluate interior node */
		Move moves[M];
		int n, nmove = generate_moves(board, moves);
		int next_lo = -hi;
		int next_hi = (-res < -lo) ? -res : -lo;

		if (best) *best = move_pass;
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
			if (best != NULL) shuffle_moves(moves, nmove);

			/* Move ordering: */
			if (ai_use_mo) order_moves(board, moves, nmove);

			/* Killer heuristic: */
			if (ai_use_killer && !move_passes(&killer)) {
				move_to_front(moves, nmove, killer);
			}

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
		assert(best == NULL || !move_passes(best));
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
		fprintf(stderr, "val=%d\n", val);
	} else {
		/* Increment max depth during placement phase: */
		static int depth = 2;
		int moves_left = max_moves_left(board);
		double start = time_used(), left = time_left();
		double used = 0, budget = left/((moves_left - 5)/2 + 1);

		fprintf(stderr, "[%.3fs] %.3fs left for %d moves; budget is %.3fs.\n",
			start, left, moves_left, budget);

		/* Killer heuristic is most effective when the transposition table
		   contains the information from one ply ago, instead of two plies: */
		if (ai_use_killer) --depth;
		
		eval_count_reset();
		aborted = false;
		for (;;) {
			/* DFS for best value and move: */
			Move new_move;
			val_t new_val = dfs(board, depth, 0, min_val, max_val, &new_move);

			if (aborted) {
				fprintf(stderr, "WARNING: search aborted!\n");
				--depth;
				break;
			}

			/* Update new best value and move: */
			val = new_val;
			*move = new_move;

			/* Report time used: */
			used = time_used() - start;
			fprintf(stderr, "[%.3fs] nmove=%d depth=%d val=%d evaluated: %d"
				" move: %s (%.3fs used)\n", time_used(), nmove, depth, val,
				eval_count_total(), format_move(move), used);

			if (used > budget) {
				fprintf(stderr, "WARNING: over budget!\n");
				--depth;
				break;
			}

			/* Determine whether to do another pass at increased search depth: */
			if (depth == max_depth || used > 0.25*budget) break;
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
