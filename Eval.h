#ifndef EVAL_H_INCLUDED
#define EVAL_H_INCLUDED

#include "Game.h"

/* Type of values used to evaluate game positions: */
#define VAL_FMT "%d"
typedef signed short val_t;
extern const val_t min_val, max_val;

/* Counters for how many positions are evaluated: */
extern int eval_count_placing;   /* placing phase */
extern int eval_count_stacking;  /* stacking phase */
extern int eval_count_end;       /* end of game */

/* Board evaluation functions, used by AI: */
EXTERN val_t eval_placing(const Board *board);
EXTERN val_t eval_end(const Board *board);
EXTERN val_t eval_stacking(const Board *board);
EXTERN val_t eval_intermediate(const Board *board);

/* Convenience functions: */
#define eval_count_reset() \
	do eval_count_placing = eval_count_stacking = eval_count_end = 0; while (0)
#define eval_count_total() \
	(eval_count_placing + eval_count_stacking + eval_count_end)

#endif /* ndef EVAL_H_INCLUDED */
