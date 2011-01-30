#include "MO.h"
#include "AI.h"
#include "Eval.h"

static void swap_moves(Move *a, Move *b)
{
	Move tmp = *a;
	*a = *b;
	*b = tmp;
}

void shuffle_moves(Move *moves, int n)
{
	while (n > 1) {
		int m = rand()%n--;
		swap_moves(&moves[m], &moves[n]);
	}
}

void move_to_front(Move *moves, int nmove, Move killer)
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

/* Principle: moves onto the opponent's stacks are good, moves onto your own
   stacks are bad, moves onto Dvonn stones somewhere in between.

   Note that unlike earlier implementations, this version of the code is
   unstable in the sense that it does not preserve the relative order of
   equally good moves! */
static void order_heuristically(const Board *board, Move *moves, int nmove)
{
	Move *i, *j, *k;

	if (board->moves <= N) return;  /* no heuristic for the placement phase! */

	/* Invariants:
		- moves <= i <= k <= j <= moves + nmove
		- moves[ ..i) are good
		- moves[i..k) are medium
		- moves[k..j) have not yet been evaluated
		- moves[j.. ) are bad
		Initially, i = k = moves, j = moves + nmove.
		Eventually, k == j. */
	i = moves, j = moves + nmove;
	for (k = i; k < j; ) {
		int discr = board->fields[k->src].player ^ board->fields[k->dst].player;
		if (discr > 0) swap_moves(i++, k++);           /* good: move to front */
		else if (discr == 0) swap_moves(k, --j);         /* bad: move to back */
		else ++k;                              /* medium: leave in the middle */
	}
}

/* New ordering function that execute all moves and directly evaluates the
   resulting positions. This is relatively expensive but gives good ordering. */
static void order_evaluated(Board *board, Move *moves, int nmove)
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

void order_moves(const Board *board, Move *moves, int nmove)
{
	if (ai_use_mo == 1) return order_heuristically(board, moves, nmove);
	if (ai_use_mo == 2) return order_evaluated((Board*)board, moves, nmove);
}
