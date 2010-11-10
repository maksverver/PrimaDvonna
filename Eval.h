#ifndef EVAL_H_INCLUDED
#define EVAL_H_INCLUDED

#include "Game.h"

/* Type of values used to evaluate game positions: */
#define VAL_FMT "%f"
typedef float val_t;

extern const val_t min_val, max_val;

typedef struct EvalWeights {
	float stacks;    /* controlled stacks (mobile or not) */
	float score;     /* pieces in controlld stacks (mobile or not) */
	float moves;     /* moves including for immobile stacks */
	float to_life;   /* moves from mobile stacks to Dvonn stones */
	float to_enemy;  /* moves from mobile stacks to enemy stones */
} EvalWeights;

extern EvalWeights eval_weights;

/* Board evaluation functions, used by AI: */
EXTERN val_t eval_placing(const Board *board);
EXTERN val_t eval_stacking(const Board *board);
EXTERN val_t eval_end(const Board *board);

#endif /* ndef EVAL_H_INCLUDED */
