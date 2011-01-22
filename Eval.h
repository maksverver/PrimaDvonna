#ifndef EVAL_H_INCLUDED
#define EVAL_H_INCLUDED

#include "Game.h"

/* Type of values used to evaluate game positions: */
#define VAL_FMT "%d"
typedef int val_t;

#define val_min ((val_t)-1000000000)
#define val_max ((val_t)+1000000000)
#define val_eps ((val_t)          1)
#define val_big ((val_t)   10000000)

#define EVAL_DEFAULT_WEIGHT_STACKS     100
#define EVAL_DEFAULT_WEIGHT_MOVES       25
#define EVAL_DEFAULT_WEIGHT_TO_LIFE     20
#define EVAL_DEFAULT_WEIGHT_TO_ENEMY    20
#define EVAL_DEFAULT_WEIGHT_FIELD_BASE  80
#define EVAL_DEFAULT_WEIGHT_FIELD_BONUS 40
#define EVAL_DEFAULT_WEIGHT_FIELD_SHIFT  4

#ifdef FIXED_PARAMS

#define EVAL_WEIGHT_STACKS       EVAL_DEFAULT_WEIGHT_STACKS
#define EVAL_WEIGHT_MOVES        EVAL_DEFAULT_WEIGHT_MOVES
#define EVAL_WEIGHT_TO_LIFE      EVAL_DEFAULT_WEIGHT_TO_LIFE
#define EVAL_WEIGHT_TO_ENEMY     EVAL_DEFAULT_WEIGHT_TO_ENEMY
#define EVAL_WEIGHT_FIELD_BASE   EVAL_DEFAULT_WEIGHT_FIELD_BASE
#define EVAL_WEIGHT_FIELD_BONUS  EVAL_DEFAULT_WEIGHT_FIELD_BONUS
#define EVAL_WEIGHT_FIELD_SHIFT  EVAL_DEFAULT_WEIGHT_FIELD_SHIFT

#else /* ndef FIXED_PARAMS */

typedef struct EvalWeights {
	val_t stacks;    /* controlled stacks (mobile or not) */
	val_t moves;     /* moves including for immobile stacks */
	val_t to_life;   /* moves from mobile stacks to Dvonn stones */
	val_t to_enemy;  /* moves from mobile stacks to enemy stones */
	val_t field_base;   /* base value of fields */
	val_t field_bonus;  /* additional Dvonn-distance-dependent bonus */
	val_t field_shift;  /* by how much to shift bonus dependent on distance */
} EvalWeights;

extern EvalWeights eval_weights;

#define EVAL_WEIGHT_STACKS       eval_weights.stacks
#define EVAL_WEIGHT_MOVES        eval_weights.moves
#define EVAL_WEIGHT_TO_LIFE      eval_weights.to_life
#define EVAL_WEIGHT_TO_ENEMY     eval_weights.to_enemy
#define EVAL_WEIGHT_FIELD_BASE   eval_weights.field_base
#define EVAL_WEIGHT_FIELD_BONUS  eval_weights.field_bonus
#define EVAL_WEIGHT_FIELD_SHIFT  eval_weights.field_shift

#endif

/* Returns how well the Dvonns are spread over the board. This is measured as
   the sum of the squared distances of each field to the nearest Dvonn stone. */
int eval_dvonn_spread(const Board *board);

/* Evaluate board position in placement phase (called by AI). */
val_t eval_placing(const Board *board);

/* Evaluate board position in stacking phase (called by AI).

   If the `board' does not desribe an end-game position, *exact is set to false;
   otherwise, it is left unchanged. */
val_t eval_stacking(const Board *board, bool *exact);

#endif /* ndef EVAL_H_INCLUDED */
