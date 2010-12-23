#ifndef EVAL_H_INCLUDED
#define EVAL_H_INCLUDED

#include "Game.h"

/* Type of values used to evaluate game positions: */
#define VAL_FMT "%d"
typedef int val_t;

extern const val_t val_min, val_max, val_eps;

typedef struct EvalWeights {
	int stacks;    /* controlled stacks (mobile or not) */
	int score;     /* pieces in controlld stacks (mobile or not) */
	int moves;     /* moves including for immobile stacks */
	int to_life;   /* moves from mobile stacks to Dvonn stones */
	int to_enemy;  /* moves from mobile stacks to enemy stones */
} EvalWeights;

extern EvalWeights eval_weights;

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
