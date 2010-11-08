#ifndef EVAL_H_INCLUDED
#define EVAL_H_INCLUDED

#include "Game.h"

/* Type of values used to evaluate game positions: */
#define VAL_FMT "%f"
typedef float val_t;

/* Weights used during evaluation of stacking moves: */
typedef struct EvalWeights {
	float stack_base;              /* base value of each stack */
	float move_base;               /* base value of each move */
	float move_opponent;           /* additional value of moving to oponnent */
	float move_near_dvonn_mult;    /* bonus value when moving to Dvonn */
	float move_near_dvonn_base;    /* bonus value decay based on distance */
	float move_immobile;           /* multiplier when move is immobile */
	float stack_immobile;          /* multiplier when sack is immobile */
	float pieces_mult;             /* bonus value for pieces on Dvonn */
	float pieces_base;             /* bonus value decay based on distance */
} EvalWeights;

extern const val_t min_val, max_val;
extern EvalWeights weights;

/* Board evaluation functions, used by AI: */
EXTERN val_t eval_placing(const Board *board);
EXTERN val_t eval_stacking(const Board *board);
EXTERN val_t eval_end(const Board *board);

#endif /* ndef EVAL_H_INCLUDED */
