#ifndef AI_H_INCLUDED
#define AI_H_INCLUDED

#include "Game.h"
#include "Eval.h"

/* Search algorithm parameters: */
extern bool ai_use_tt;      /* use transposition table? */
extern bool ai_use_mo;      /* use move reordering? */
extern bool ai_use_killer;  /* use killer heuristic? */

/* Selects the next move to make during the movement phase. */
EXTERN bool ai_select_move(Board *board, Move *move);

/* Same as above, but uses fixed search depth. */
EXTERN bool ai_select_move_fixed(Board *board, Move *move, int depth);

/* Evaluates the current board. Mainly useful for analysis/debugging. */
EXTERN val_t ai_evaluate(const Board *board);

/* Attempts to extract the first `nmove' moves of the principal variation for
   the given state from the transposition table and returns how many moves
   could be extracted, which may be less than requested. */
EXTERN int ai_extract_pv(Board *board, Move *moves, int nmove);

#endif /* ndef AI_H_INCLUDED */
