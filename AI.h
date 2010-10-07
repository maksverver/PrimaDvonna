#ifndef AI_H_INCLUDED
#define AI_H_INCLUDED

#include "Game.h"

/* Type of values used to evaluate game positions: */
typedef signed short val_t;
EXTERN const val_t min_val, max_val;

/* Toggles whether to use the transposition table: */
extern bool ai_use_tt;

/* Selects the next move to make during the movement phase. */
EXTERN bool ai_select_move(Board *board, Move *move);

/* Same as above, but uses fixed search depth. */
EXTERN bool ai_select_move_fixed(Board *board, Move *move, int depth);

/* Evaluates the current board. Mainly useful for analysis/debugging. */
EXTERN val_t ai_evaluate(const Board *board);

#endif /* ndef AI_H_INCLUDED */
