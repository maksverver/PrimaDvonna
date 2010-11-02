#ifndef AI_H_INCLUDED
#define AI_H_INCLUDED

#include "Game.h"
#include "Eval.h"

/* Search algorithm parameters: */
extern bool ai_use_tt;      /* use transposition table? */
extern bool ai_use_mo;      /* use move reordering? */
extern bool ai_use_killer;  /* use killer heuristic? */

/* Selects the next move to make during the movement phase.

   Uses iterative deepening negamax search with various optimizations.

   If max_time > 0, then searching is limited by time, to the point that any
   iteration beyond the first may be aborted halfway through if necessary to
   meet the deadline, though will typically return before the deadline.

   If min_eval > 0, then the search is never aborted, but instead iterative
   deepening continues until at least min_eval positions have been evaluated
   (or when the maximum search depth has been reached).

   Finally, if min_depth > 0, then iterative deepening continues until the
   search depth equals at least min_depth (or the maximum search depth has been
   reached).

   Note that this function always does at least one pass at some internally
   determined reasonable starting depth. This means in particular that the first
   iteration may be at a higher level than max_depth and use more time than
   max_time!

   Returns `true' when a move is available. In that case, if `best_move',
   `best_val' and `best_depth' are not NULL, they are used to store the best
   move, its value and the maximum search depth used. */
EXTERN bool ai_select_move( Board *board,
	double max_time, int min_eval, int min_depth,
	Move *best_move, val_t *best_value, int *best_depth );


/* Evaluates the current board. Mainly useful for analysis/debugging. */
EXTERN val_t ai_evaluate(const Board *board);

/* Attempts to extract the first `nmove' moves of the principal variation for
   the given state from the transposition table and returns how many moves
   could be extracted, which may be less than requested. */
EXTERN int ai_extract_pv(Board *board, Move *moves, int nmove);

#endif /* ndef AI_H_INCLUDED */
