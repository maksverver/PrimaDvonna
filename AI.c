#include "AI.h"
#include "IO.h"
#include "MO.h"
#include "Signal.h"
#include "Time.h"
#include "TT.h"
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
static int eval_count = 0;
static int rng_seed = 0;

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

   If `return_best' is not NULL, then the best move is assigned to *return_best.

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
static val_t dfs(Board *board, int depth, int pass, int lo, int hi,
                 Move *return_best)
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
				(!return_best || valid_move(board, &entry->killer))) {
				if (return_best) *return_best = entry->killer;
				if (entry->lo == entry->hi) return entry->lo;
				if (entry->lo >= hi) return entry->lo;
				if (entry->hi <= lo) return entry->hi;
				res = entry->lo;
			}
			if (ai_use_killer) killer = entry->killer;
		}
	}
	if (pass == 2) {  /* evaluate end position */
		assert(!return_best);
		++eval_count;
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
		assert(!return_best);
		res = ai_evaluate(board);
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

		if (return_best) *return_best = move_null;  /* for integrity checking */
		if (nmove == 1) {
			/* Only one move available: */
			killer = moves[0];
			board_do(board, &moves[0]);
			res = -dfs(board, depth, (move_passes(&moves[0]) ? pass+1 : 0),
					   next_lo, next_hi, NULL);
			board_undo(board, &moves[0]);
			if (aborted) return 0;
		} else {
			/* Multiple moves available */
			assert(nmove > 1);

			if (return_best) {
				/* Randomize moves in a semi-random fashion: */
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
		if (return_best) *return_best = killer;
		assert(!return_best || !move_is_null(return_best));
	}
	return res;
}

static void set_aborted()
{
	aborted = true;
}

EXTERN bool ai_select_move( Board *board,
	const AI_Limit *limit, AI_Result *result )
{
	static int depth = 2;  /* iterative deepening start depth */

	signal_handler_t new_handler, old_handler;
	double start = time_used();
	bool alarm_set = false, signal_handler_set = false;
	Move moves[M];
	int nmove = generate_moves(board, moves);

	/* Check if we have any moves to make: */
	if (nmove == 0) {
		fprintf(stderr, "no moves available!\n");
		return false;
	}
	if (nmove == 1) fprintf(stderr, "one move available!\n");

	/* Killer heuristic is most effective when the transposition table
	   contains the information from one ply ago, instead of two plies: */
	if (ai_use_killer && depth > 2) --depth;

	eval_count = 0;
	aborted = false;
	for (;;) {
		/* DFS for best value and move: */
		Move move;
		val_t value = dfs(board, depth, 0, min_val, max_val, &move);
		double used = time_used() - start;

		if (aborted) {
			if (result) {
				result->aborted = true;
				result->time = used;
			}
			fprintf(stderr, "WARNING: aborted after %.3fs!\n", used);
			--depth;
			break;
		}

		/* Update results so far: */
		if (result) {
			result->move    = move;
			result->depth   = depth;
			result->value   = value;
			result->eval    = eval_count;
			result->time    = used;
			result->aborted = false;
		}

		/* Report intermediate result: */
		if (board->moves >= N) {
			fprintf(stderr, "m:%s d:%d v:"VAL_FMT" e:%d u:%.3fs\n",
				format_move(&move), depth, value, eval_count, used);
		}

		if (limit && limit->time > 0 && used > limit->time) {
			/* N.B. if this happens during CodeCup games, we could time out! */
			fprintf(stderr, "WARNING: max_time exceeded!\n");
			--depth;
			break;
		}

		/* Determine whether to search again with increased depth: */
		if (depth == max_depth || nmove == 1) break;
		if (limit) {
			if (limit->eval > 0 && eval_count >= limit->eval) break;
			if (limit->depth > 0 && depth >= limit->depth) break;
			if (limit->time > 0) {
				if (used >= ((depth%2 == 0) ? 0.1 : 0.4)*limit->time) break;
				if (!alarm_set++) {
					set_alarm(limit->time - used, set_aborted, NULL);
				}
			}
		}
		if (!signal_handler_set++) {
			signal_handler_init(&new_handler, set_aborted);
			signal_swap_handlers(SIGINT, &new_handler, &old_handler);
		}
		++depth;
	}
	if (alarm_set) clear_alarm();
	if (signal_handler_set) signal_swap_handlers(SIGINT, &old_handler, NULL);
	aborted = false;
	return true;
}

EXTERN val_t ai_evaluate(const Board *board)
{
	++eval_count;
	if (board->moves >= N) {
		return eval_stacking(board);
	} else if (board->moves > D) {
		return eval_placing(board);
	} else {  /* board->moves <= D */
		return 0;
	}
}

EXTERN int ai_extract_pv(Board *board, Move *moves, int nmove)
{
	int n;
	hash_t hash;
	TTEntry *entry;

	for (n = 0; n < nmove && generate_all_moves(board, NULL) > 0 ; ++n)
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
