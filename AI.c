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

/* Search algorithm parameters: (defaults set here perform best) */
int ai_use_tt     = 21;  /* size as a power of 2*/
int ai_use_mo     = 1;
int ai_use_killer = 1;

/* Global flag to abort search: */
static volatile bool aborted = false;
static int eval_count = 0;

static TTEntry *tt_entry(hash_t hash)
{
	return &tt[(size_t)(hash ^ (hash >> 32))&(tt_size - 1)];
}

/* Implements depth-first minimax search with alpha-beta pruning.

   Takes the current game state in `board' and the desired maximum search depth
   `depth' and returns the value of the game position in the range [lo+1:hi-1],
   or if the value is <= lo, then it returns an upper bound on the game value,
   or if the value is >= hi, then it returns a lower bound on the value.

   If `return_best' is not NULL, then the best move is assigned to *return_best.

   Note that it tries to return as tight a bound as possible without evaluating
   more positions than stricly necessary. This enables re-using transposition
   table entries as often as possible.

   Note that the search may be aborted by setting the global variable `aborted'
   to `true'. In that case, dfs() returns 0, and the caller (which includes
   dfs() itself) must ensure that the value is not used as a valid result! This
   means that all calls to dfs() should be followed by checking `aborted' before
   using the return value.
*/
static val_t dfs(Board *board, int depth, val_t lo, val_t hi, Move *return_best)
{
	hash_t hash = (hash_t)-1;
	IF_TT_DEBUG( unsigned char data[50] )
	TTEntry *entry = NULL;
	val_t res = val_min;
	Move best_move = move_null;

	if (ai_use_tt) { /* look up in transposition table: */
		hash = hash_board(board);
		IF_TT_DEBUG( serialize_board(board, data) )
		entry = tt_entry(hash);
		IF_TT_DEBUG( ++tt_stats.queries )
		IF_TT_DEBUG( if (entry->hash != hash) ++tt_stats.missing )
		if (entry->hash == hash) {
			/* detect hash collisions */
			IF_TT_DEBUG( assert(memcmp(entry->data, data, 50) == 0) )
			IF_TT_DEBUG( if (entry->depth != depth) ++tt_stats.shallow )
			/* We could use >= depth here too, but that leads to search
			   instability, which might better be avoided. */
			if (entry->depth == depth &&
				(!return_best || valid_move(board, &entry->killer))) {
				if (return_best) *return_best = entry->killer;
				if (entry->lo == entry->hi) return entry->lo;
				if (entry->lo >= hi) return entry->lo;
				if (entry->hi <= lo) return entry->hi;
				res = entry->lo;
				IF_TT_DEBUG( ++tt_stats.partial )
			}
			best_move = entry->killer;
		}
	}
	if (depth == 0) {  /* evaluate intermediate position */
		res = ai_evaluate(board);
	} else {  /* evaluate interior node */
		Move moves[M];
		int n, nmove;

		nmove = generate_moves(board, moves);
		if (nmove > 1) {  /* order moves */

			/* At the top level, shuffle moves in a semi-random fashion: */
			if (return_best) {
				static int rng_seed = 0;
				while (rng_seed == 0) rng_seed = rand();
				srand(rng_seed);
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
			val_t val;

			board_do(board, &moves[n]);
			if (board->moves == N) { /* after N'th move, white moves again: */
				val = dfs(board, depth - 1, lo > res ? lo : res, hi, NULL);
			} else { /* in all other cases, player's turns alternate: */
				val = -dfs(board, depth - 1, -hi, -(lo > res ? lo : res), NULL);
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
		int relevance = board->moves + 2*depth;

		IF_TT_DEBUG( ++tt_stats.updates )
		if (relevance < entry->relevance) {a
			IF_TT_DEBUG( ++tt_stats.discarded )
		} else {
			IF_TT_DEBUG(
				if (entry->hash != 0) {
					if (entry->hash != hash) ++tt_stats.overwritten;
					else if (entry->depth == depth) ++tt_stats.updated;
					else ++tt_stats.upgraded;
				} )

			/* FIXME: eval_intermediate() could end up calling eval_end() if
				the current position is actually an end position. In that case,
				we should really store depth = AI_MAX_DEPTH here! */
			entry->hash      = hash;
			entry->lo        = (depth == 0 || res > lo) ? res : val_min;
			entry->hi        = (depth == 0 || res < hi) ? res : val_max;
			entry->depth     = depth;
			entry->relevance = relevance;
			entry->killer    = best_move;
			IF_TT_DEBUG( memcpy(entry->data, data, 50) )
		}
	}
	if (return_best) *return_best = best_move;
	return res;
}

static void set_aborted()
{
	aborted = true;
}

EXTERN bool ai_select_move( Board *board,
	const AI_Limit *limit, AI_Result *result )
{
	static int depth = 1;  /* iterative deepening start depth */

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
	if (ai_use_tt && ai_use_killer == 1 && depth > 2) --depth;

	if (limit->depth > 0 && limit->depth < depth) depth = limit->depth;

	eval_count = 0;
	aborted = false;
	for (;;) {
		/* DFS for best value and move: */
		Move move = move_null;
		val_t value = dfs(board, depth, val_min, val_max, &move);
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

		assert(!move_is_null(&move));

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
		if (depth == AI_MAX_DEPTH || nmove == 1) break;
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
