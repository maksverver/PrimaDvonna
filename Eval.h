#ifndef EVAL_H_INCLUDED
#define EVAL_H_INCLUDED

#include "Game.h"

/* Type of values used to evaluate game positions: */
#define VAL_FMT "%d"
typedef signed short val_t;
extern const val_t min_val, max_val;

/* Board evaluation functions, used by AI: */
EXTERN val_t eval_placing(const Board *board);
EXTERN val_t eval_stacking(const Board *board);
EXTERN val_t eval_end(const Board *board);

#endif /* ndef EVAL_H_INCLUDED */
