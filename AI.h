#ifndef AI_H_INCLUDED
#define AI_H_INCLUDED

#include "Game.h"
#include "Eval.h"

/* Maximum search depth: */
#define AI_MAX_DEPTH 32

/* Search algorithm parameters: */
#define AI_DEFAULT_TT       21
#define AI_DEFAULT_MO       1
#define AI_DEFAULT_KILLER   1
#define AI_DEFAULT_PVS      1

#ifdef FIXED_PARAMS
#define ai_use_tt     AI_DEFAULT_TT
#define ai_use_mo     AI_DEFAULT_MO
#define ai_use_killer AI_DEFAULT_KILLER
#define ai_use_pvs    AI_DEFAULT_PVS
#else  /* ndef FIXED_PARAMS */
extern int ai_use_tt;      /* size as a power of 2, or 0 to disable */
extern int ai_use_mo;      /* use move reordering? (0, 1 or 2) */
extern int ai_use_killer;  /* use killer heuristic? (0 or 1) */
extern int ai_use_pvs;     /* use principal variation search? (0 or 1) */
#endif

/* Limits on the search performed by the AI when selecting moves.

   If max_time > 0, then searching is limited by time, to the point that any
   iteration beyond the first may be aborted halfway through if necessary to
   meet the deadline, though will typically return before the deadline.

   If min_eval > 0, then the search is never aborted, but instead iterative
   deepening continues until at least min_eval positions have been evaluated
   (or when the maximum search depth has been reached).

   Finally, if min_depth > 0, then iterative deepening continues until the
   search depth equals at least min_depth (or the maximum search depth has been
   reached).

   Note that these limits are cumulative, in the sense that the first bound
   that is reached terminates the search, so setting more fields to positive
   values causes the search to be aborted sooner.
*/
typedef struct AI_Limit {
	int    depth;  /* stop after reaching or exceeding given depth */
	int    eval;   /* stop after evaluating given number of positions */
	double time;   /* maximum time to search */
} AI_Limit;

/* Results of searching for the best move.

   A search may be aborted by the time limit or by a keyboard interrupt; in that
   case, the time used and number of positions evaluated will be larger than
   would be necessary to search to the returned depth, because search was
   aborted inside the next depth.
*/
typedef struct AI_Result {
	Move   move;     /* selected move */
	val_t  value;    /* value of the move at the last completed search */
	int    depth;    /* maximum search depth completed */
	int    eval;     /* total number of positions evaluated */
	double time;     /* total time used */
	bool   aborted;  /* whether search was aborted */
	bool   exact;    /* whether the entire game tree was searched */
} AI_Result;

/* Selects the next best move to make.

   Uses iterative deepening negamax search with various optimizations. If
   `limit' is non-NULL, it specifies the time/depth/eval limits on the search,
   as described as above.

   The result of the search (most importantly the selected move) is stored in
   `result'.

   This function returns false only if there are no moves to make. */
bool ai_select_move( Board *board,
	const AI_Limit *limit, AI_Result *result );

/* Evaluates the current board. Mainly useful for analysis/debugging. */
val_t ai_evaluate(const Board *board);

/* Attempts to extract the first `nmove' moves of the principal variation for
   the given state from the transposition table and returns how many moves
   could be extracted, which may be less than requested. */
int ai_extract_pv(Board *board, Move *moves, int nmove);

#endif /* ndef AI_H_INCLUDED */
