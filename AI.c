#include "AI.h"
#include "IO.h"
#include "MO.h"
#include "Signal.h"
#include "Time.h"
#include "TT.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef TT_DEBUG
#define IF_TT_DEBUG(x) x;
#else
#define IF_TT_DEBUG(x)
#endif

#ifndef FIXED_PARAMS
int ai_use_tt         = AI_DEFAULT_TT;
int ai_use_mo         = AI_DEFAULT_MO;
int ai_use_killer     = AI_DEFAULT_KILLER;
int ai_use_pvs        = AI_DEFAULT_PVS;
int ai_use_mtdf       = AI_DEFAULT_MTDF;
int ai_use_deepening  = AI_DEFAULT_DEEPENING;
#endif

/* Global flag to abort search: */
static volatile bool aborted = false;

/* Number of states evaluated since last call to ai_select_move(): */
static int eval_count = 0;

/* Returns the transposition table entry for the given hash code. */
static TTEntry *tt_entry(hash_t hash)
{
	TTEntry *entry = &tt[(size_t)(hash ^ (hash >> 32))&(tt_size - 1)];
#ifdef PROBING
	int max_tries = 16;
	while (entry->hash && entry->hash != hash) {
		if (--max_tries > 0) {
			/* N.B. this is technically undefined behaviour: */
                        entry -= hash&15;
			if (entry < tt) entry += tt_size;
			break;
		}
		if (++entry == &tt[tt_size]) entry = tt;
	}
#endif
	return entry;
}

/* Resets the random number generator, using a fixed (but randomly chosen) seed.
   This is a hack used to guarantee that functions like ai_select_move() are
   deterministic, but unpredictable, so that the games played by the AI vary
   a bit (even if the opponent's moves do not). */
static void reset_rng()
{
	static int rng_seed = 0;
	while (rng_seed == 0) rng_seed = rand();
	srand(rng_seed);
}

/* Evaluates the current board by calling the appropriate function depending
   on the game phase. If the game value is exact, *exact is set to true;
   otherwise, it is left unmodified. */
static val_t evaluate(const Board *board, bool *exact)
{
	++eval_count;
	if (board->moves >= N) {  /* stacking phase */
		return eval_stacking(board, exact);
	} else {  /* placement phase */
		*exact = false;
		if (board->moves > D) {  /* some player's pieces placed */
			return eval_placing(board);
		} else {  /* only Dvonn pieces placed; too early to evaluate */
			*exact = false;
			return 0;
		}
	}
}

/* Implements depth-first minimax search with (fail soft) alpha-beta pruning.

   Takes the current game state in `board' and the desired maximum search depth
   `depth' and returns the value of the game position in the range [lo+1:hi-1],
   or if the value is <= lo, then it returns an upper bound on the game value,
   or if the value is >= hi, then it returns a lower bound on the value.

   If `return_best' is not NULL, then the best move is assigned to *return_best.

   If the game value is not exact (that is, some mid-game positions were
   evaluated to determine it) then *return_exact is set to false. If the value
   is exact (only end-game positions were evaluated) it is left unchanged.

   This function tries to return as tight a bound as possible without evaluating
   more positions than stricly necessary. This enables re-using transposition
   table entries as often as possible.

   Note that the search may be aborted by setting the global variable `aborted'
   to `true'. In that case, dfs() returns 0, and the caller (which includes
   dfs() itself) must ensure that the value is not used as a valid result! This
   means that all calls to dfs() should be followed by checking `aborted' before
   using the return value.
*/
static val_t dfs( Board *board, int depth, val_t lo, val_t hi,
                  Move *return_best, bool *return_exact)
{
	hash_t hash = (hash_t)-1;
	IF_TT_DEBUG( unsigned char data[50] )
	TTEntry *entry = NULL;
	val_t res = val_min;
	Move best_move = move_null;
	bool exact = true;

	assert(lo < hi);

	if (ai_use_tt) { /* look up in transposition table: */
		hash = hash_board(board);
		IF_TT_DEBUG( serialize_board(board, data) )
		entry = tt_entry(hash);
		IF_TT_DEBUG( ++tt_stats.queries )
		IF_TT_DEBUG( if (entry->hash != hash) ++tt_stats.missing )
		if (entry->hash == hash) {
			/* detect hash collisions */
			IF_TT_DEBUG( assert(memcmp(entry->data, data, 50) == 0) )
			IF_TT_DEBUG( if ( entry->depth <= AI_MAX_DEPTH &&
			                  entry->depth != depth ) ++tt_stats.shallow )
			/* We could use >= depth here too, but that leads to search
			   instability, which might better be avoided. */
			if ((entry->depth == depth || entry->depth > AI_MAX_DEPTH) &&
			    (!return_best || valid_move(board, &entry->killer)))
			{
				if (entry->lo == entry->hi || entry->lo >= hi) {
					if (return_best) *return_best = entry->killer;
					if (entry->depth <= AI_MAX_DEPTH) *return_exact = false;
					return entry->lo;
				} else if (entry->hi <= lo) {
					if (return_best) *return_best = entry->killer;
					if (entry->depth <= AI_MAX_DEPTH) *return_exact = false;
					return entry->hi;
				}
				if (entry->lo > lo) lo = entry->lo;  /* dubious? */
				if (entry->hi < hi) hi = entry->hi;  /* dubious? */
				IF_TT_DEBUG( ++tt_stats.partial )
			}
			best_move = entry->killer;
		}
	}
	if (depth == 0) {  /* evaluate intermediate position */
		res = evaluate(board, &exact);
	} else if (board->moves == N - 1) {
		/* Special case: the N'th move is always unique, but the next player
		   does not change! Handle this special case here: */
		Move moves[M];
		int nmove = generate_moves(board, moves);
		assert(nmove == 1);
		board_do(board, &moves[0]);
		res = dfs(board, depth, lo > res ? lo : res, hi, NULL, &exact);
		board_undo(board, &moves[0]);
		if (aborted) return 0;
		best_move = moves[0];
	} else {  /* evaluate interior node */
		Move moves[M];
		int n, nmove = generate_moves(board, moves);

		if (nmove > 1) {  /* order moves */

			/* At the top level, shuffle moves in a semi-random fashion: */
			if (return_best) {
				reset_rng();
				shuffle_moves(moves, nmove);
			}

			/* Move ordering: */
			if (ai_use_mo && (ai_use_mo < 2 || depth > 1)) {
				order_moves(board, moves, nmove);
			}

			/* Killer heuristic: */
			if (ai_use_killer && !move_is_null(&best_move)) {
				move_to_front(moves, nmove, best_move);
			}
		}

		for (n = 0; n < nmove; ++n) {
			val_t val, lb = (res > lo) ? res : lo;

			board_do(board, &moves[n]);
			if (!ai_use_pvs || n == 0 || res < lo) {
				val = -dfs(board, depth - 1, -hi, -lb, NULL, &exact);
			} else {
				val = -dfs(board, depth - 1, -lb - val_eps, -lb, NULL, &exact);
				if (val > lb && val < hi) {
					val = -dfs(board, depth - 1, -hi, -val, NULL, &exact);
				}
			}
			board_undo(board, &moves[n]);
			if (aborted) return 0;

			/* Update value bounds: */
			if (val > res) {
				res = val;
				best_move = moves[n];
				if (res >= hi) break;
			}
		}
	}
	if (ai_use_tt) {
		/* Replacement policy: replace existing position with a new one if its
		   relevance is greater or equal, where relevance is defined as: */
		int eff_depth = exact ? AI_MAX_DEPTH + 1 : depth;
		int relevance = board->moves + 2*eff_depth;

		IF_TT_DEBUG( ++tt_stats.updates )
		if (relevance < entry->relevance) {
			IF_TT_DEBUG( ++tt_stats.discarded )
		} else {
			IF_TT_DEBUG(
				if (entry->hash != 0) {
					if (entry->hash != hash) ++tt_stats.overwritten;
					else if (entry->depth == depth) ++tt_stats.updated;
					else ++tt_stats.upgraded;
				} )

			if (entry->hash != hash || entry->depth != eff_depth)
			{
				entry->hash  = hash;
				entry->lo    = val_min;
				entry->hi    = val_max;
				entry->depth = eff_depth;
			}
			if ((depth == 0 || res > lo) && res > entry->lo) entry->lo = res;
			if ((depth == 0 || res < hi) && res < entry->hi) entry->hi = res;
			entry->relevance = relevance;
			entry->killer    = best_move;
			IF_TT_DEBUG( memcpy(entry->data, data, 50) )
		}
	}
	if (!exact) *return_exact = false;
	if (return_best) *return_best = best_move;
	return res;
}

/* Callback handler for the timeout alarm. */
static void set_aborted()
{
	aborted = true;
}

bool ai_select_move( Board *board,
	const AI_Limit *limit, AI_Result *result )
{
	static int depth = 1;  /* iterative deepening start depth */

	signal_handler_t new_handler, old_handler;
	Move moves[M];
	int nmove = generate_moves(board, moves);
	double start = time_used();
	double prev_used = 0.0;
	double ratio = 5.0;
	bool alarm_set = false, signal_handler_set = false;

	/* Check if we have any moves to make: */
	if (nmove == 0) {
		fprintf(stderr, "no moves available!\n");
		return false;
	}
	if (nmove == 1) fprintf(stderr, "one move available!\n");

	/* Initialize result */
	result->move    = move_null;
	result->value   = 0;
	result->depth   = 0;
	result->eval    = 0;
	result->time    = 0;
	result->aborted = false;
	result->exact   = false;

	/* Special handling for placing of neutral Dvonn stones: */
	if (board->moves < D) {
		reset_rng();
		shuffle_moves(moves, nmove);
		if (board->moves == 0) {
			/* Place first Dvonn randomly */
			result->move = moves[0];
		} else {
			/* Place second/third Dvonn to minimize distance to Dvonns: */
			int n, val, best_val = -1;
			for (n = 0; n < nmove; ++n) {
				board_do(board, &moves[n]);
				val = eval_dvonn_spread(board);
				board_undo(board, &moves[n]);
				if (best_val == -1 || val < best_val) {
					result->move = moves[n];
					best_val = val;
				}
			}
			result->depth = 1;
			result->eval  = nmove;
		}
		return true;
	}

	/* Killer heuristic is most effective when the transposition table
	   contains the information from one ply ago, instead of two plies: */
	if (ai_use_tt && ai_use_killer == 1 && depth > 2) --depth;

	if (limit->depth > 0 && limit->depth < depth) depth = limit->depth;

	/* Round to least multiple of deepening increment: */
	if (depth%ai_use_deepening > 0) {
		if (depth < ai_use_deepening) {
			depth = ai_use_deepening;
		} else {
			depth += ai_use_deepening - depth%ai_use_deepening;
		}
	}

	eval_count = 0;
	aborted = false;
	for (;;) {
		/* DFS for best value and move: */
		Move move = move_null;
		bool exact = true;
		val_t value;
		double used;

		if (!ai_use_mtdf)
		{
			value = dfs(board, depth, val_min, val_max, &move, &exact);
		}
		else
		{
			val_t lo = val_min, hi = val_max;
			value = result->value;
			while (lo < hi)
			{
				val_t beta = value;
				if (beta == lo) ++beta;
				value = dfs(board, depth, beta - 1, beta, &move, &exact);
				fprintf(stderr, "[%d:%d] %d\n", lo, hi, value);
				if (value < beta) hi = value; else lo = value;
			}
			fprintf(stderr, "[%d:%d] %d\n", lo, hi, value);
		}
		used = time_used() - start;
		if (prev_used > 0) ratio = used/prev_used;
		prev_used = used;
		if (aborted) {
			result->aborted = true;
			result->time = used;
			fprintf(stderr, "WARNING: aborted after %.3fs!\n", used);
			--depth;
			break;
		}

		assert(!move_is_null(&move));

		/* Update results so far: */
		result->move    = move;
		result->depth   = depth;
		result->value   = value;
		result->eval    = eval_count;
		result->time    = used;
		result->aborted = false;
		result->exact   = exact;

		/* Report intermediate result: */
		if (board->moves >= N) {
			fprintf(stderr, "m:%s d:%d v:"VAL_FMT"%s e:%d u:%.3fs r:%.1f\n",
				format_move(&move), depth, value, exact ? " (exact)" : "",
				eval_count, used, ratio);
		}

		if (limit && limit->time > 0 && used > limit->time) {
			/* N.B. if this happens during CodeCup games, we could time out! */
			fprintf(stderr, "WARNING: max_time exceeded!\n");
			--depth;
			break;
		}

		/* Determine whether to search again with increased depth: */
		if (exact || depth == AI_MAX_DEPTH || nmove == 1) break;
		if (limit) {
			if (limit->eval > 0 && eval_count >= limit->eval) break;
			if (limit->depth > 0 && depth >= limit->depth) break;
			if (limit->time > 0) {
				double end = used*( (ai_use_deepening < 2)
					? ((depth%2 == 0) ? 2*ratio : ratio/2) : ratio*ratio );
				if (end >= limit->time) break;
				if (!alarm_set++) {
					set_alarm(limit->time - used, set_aborted, NULL);
				}
			}
		}
		if (!signal_handler_set++) {
			signal_handler_init(&new_handler, set_aborted);
			signal_swap_handlers(SIGINT, &new_handler, &old_handler);
		}
		if (!ai_use_mtdf)
		{
			++depth;
		}
		else
		{
			depth = depth + ai_use_deepening;
		}
	}
	if (alarm_set) clear_alarm();
	if (signal_handler_set) signal_swap_handlers(SIGINT, &old_handler, NULL);
#ifdef TT_DEBUG
	{
		long long pop = tt_population_count();
		fprintf(stderr, "TT stats:\n");
		fprintf(stderr, "\tqueries:       %20lld\n", tt_stats.queries);
		fprintf(stderr, "\t  missing:     %20lld\n", tt_stats.missing);
		fprintf(stderr, "\t  shallow:     %20lld\n", tt_stats.shallow);
		fprintf(stderr, "\t  partial:     %20lld\n", tt_stats.partial);
		fprintf(stderr, "\tupdates:       %20lld\n", tt_stats.updates);
		fprintf(stderr, "\t  discarded:   %20lld\n", tt_stats.discarded);
		fprintf(stderr, "\t  updated:     %20lld\n", tt_stats.updated);
		fprintf(stderr, "\t  upgraded:    %20lld\n", tt_stats.upgraded);
		fprintf(stderr, "\t  overwritten: %20lld\n", tt_stats.overwritten);
		fprintf(stderr, "\tpopulation:    %20lld (%5.2f%%)\n",
			pop, 100.0*pop/tt_size);
		assert(tt_stats.updates <=  /* inequality occurs when aborting search */
			tt_stats.missing + tt_stats.shallow + tt_stats.partial);
		assert(pop == tt_stats.updates - tt_stats.discarded -
			tt_stats.updated - tt_stats.upgraded - tt_stats.overwritten);
	}
#endif
	aborted = false;
	return true;
}

val_t ai_evaluate(const Board *board)
{
	bool dummy;
	return evaluate(board, &dummy);
}

int ai_extract_pv(Board *board, Move *moves, int nmove)
{
	int n;
	hash_t hash;
	TTEntry *entry;

	if (!ai_use_tt) return 0;

	for (n = 0; n < nmove && generate_all_moves(board, NULL) > 0 ; ++n)
	{
		hash = hash_board(board);
		entry = tt_entry(hash);
		if (entry->hash != hash || move_is_null(&entry->killer)) break;
		assert(valid_move(board, &entry->killer));
		moves[n] = entry->killer;
		board_do(board, &moves[n]);
	}
	nmove = n;
	while (n > 0) board_undo(board, &moves[--n]);
	return nmove;
}
