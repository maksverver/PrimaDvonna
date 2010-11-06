#include "MO.h"
#include "Eval.h"

static void swap_moves(Move *a, Move *b)
{
	Move tmp = *a;
	*a = *b;
	*b = tmp;
}

EXTERN void shuffle_moves(Move *moves, int n)
{
	while (n > 1) {
		int m = rand()%n--;
		swap_moves(&moves[m], &moves[n]);
	}
}

EXTERN void move_to_front(Move *moves, int nmove, Move killer)
{
	int i;

	for (i = 0; i < nmove; ++i) {
		if (move_compare(&moves[i], &killer) == 0) {
			for ( ; i > 0; --i) moves[i] = moves[i - 1];
			moves[0] = killer;
			return;
		}
	}
}

#if 0
/* Principle: moves onto the opponent's stacks are good, moves onto your own
   stacks are bad, moves onto neutral stacks (Dvonn stones) are medium.

   Note that unlike earlier implementations, this version of the code is
   unstable in the sense that it does not preserve the relative order of
   equally good moves! */
void order_moves(Board *board, Move *moves, int nmove)
{
	val_t values[M];
	int i, j;
	Move m;
	val_t v;

	/* Evaluate successors: */
	for (i = 0; i < nmove; ++i)
	{
		board_do(board, &moves[i]);
		/* Don't call ai_evaluate here, to avoid updating eval counter: */
		/*
		values[i] =
			(board->moves < D) ? 0 :
			(board->moves < N) ? eval_placing(board) : eval_stacking(board);
		*/
		extern val_t ai_evaluate(const Board *board);
		values[i] = ai_evaluate(board);
		board_undo(board, &moves[i]);
	}

	/* Insertion sort by increasing value, because the values computed above
	   are relative to the opponent, so better moves for the current player
	   will have lower values: */
	for (i = 1; i < nmove; ++i)
	{
		v = values[i];
		m = moves[i];
		for (j = i; j > 0 && values[j - 1] > v; --j) {
			values[j] = values[j - 1];
			moves[j] = moves[j - 1];
		}
		moves[j] = m;
		values[j] = v;
	}
}
#endif

/* New evaluation function that orders moves by executing them all, and
   directly evaluating the resulting games. */
void order_moves(Board *board, Move *moves, int nmove)
{
	val_t values[M];
	int i, j;
	Move m;
	val_t v;

	/* Evaluate successors: */
	for (i = 0; i < nmove; ++i)
	{
		extern val_t ai_evaluate(const Board *board);
		board_do(board, &moves[i]);
		/* Don't call ai_evaluate here, to avoid updating eval counter? */
		/*
		values[i] =
			(board->moves < D) ? 0 :
			(board->moves < N) ? eval_placing(board) : eval_stacking(board);
		*/
		values[i] = ai_evaluate(board);
		board_undo(board, &moves[i]);
	}

	/* Insertion sort by increasing value, because the values computed above
	   are relative to the opponent, so better moves for the current player
	   will have lower values: */
	for (i = 1; i < nmove; ++i)
	{
		v = values[i];
		m = moves[i];
		for (j = i; j > 0 && values[j - 1] > v; --j) {
			values[j] = values[j - 1];
			moves[j] = moves[j - 1];
		}
		moves[j] = m;
		values[j] = v;
	}
}
