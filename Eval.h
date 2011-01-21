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

#ifdef FIXED_PARAMS

#define EVAL_WEIGHT_STACKS   EVAL_DEFAULT_WEIGHT_STACKS
#define EVAL_WEIGHT_MOVES    EVAL_DEFAULT_WEIGHT_MOVES
#define EVAL_WEIGHT_TO_LIFE  EVAL_DEFAULT_WEIGHT_TO_LIFE
#define EVAL_WEIGHT_TO_ENEMY EVAL_DEFAULT_WEIGHT_TO_ENEMY

#else /* ndef FIXED_PARAMS */

typedef struct EvalWeights {
	int stacks;    /* controlled stacks (mobile or not) */
	int moves;     /* moves including for immobile stacks */
	int to_life;   /* moves from mobile stacks to Dvonn stones */
	int to_enemy;  /* moves from mobile stacks to enemy stones */
} EvalWeights;

extern EvalWeights eval_weights;

#define EVAL_WEIGHT_STACKS   eval_weights.stacks
#define EVAL_WEIGHT_MOVES    eval_weights.moves
#define EVAL_WEIGHT_TO_LIFE  eval_weights.to_life
#define EVAL_WEIGHT_TO_ENEMY eval_weights.to_enemy

#endif

/* Resets the position of the Dvonn stones according to the state in `board'.
   This must be called before every invocation of eval_placing() where the
   positions of the Dvonn stones has changed. (Called by AI as needed.) */
void eval_update_dvonns(const Board *board);

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
