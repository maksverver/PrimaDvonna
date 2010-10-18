#include "MO.h"

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

/* Principle: moves onto the opponent's stacks are good, moves onto your own
   stacks are bad, moves onto neutral stacks (Dvonn stones) are medium.

   Note that unlike earlier implementations, this version of the code is
   unstable in the sense that it does not preserve the relative order of
   equally good moves! */
void order_moves(const Board *board, Move *moves, int nmove)
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
		int discr =
			board->fields[k->r1][k->c1].player ^
			board->fields[k->r2][k->c2].player;
		if (discr > 0) swap_moves(i++, k++);           /* good: move to front */
		else if (discr == 0) swap_moves(k, --j);         /* bad: move to back */
		else ++k;                              /* medium: leave in the middle */
	}
}
