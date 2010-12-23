#include "Eval.h"
#include <math.h>
#include <assert.h>

const val_t val_min = -1000000000;
const val_t val_max = +1000000000;
const val_t val_eps =           1;
const val_t val_big =     1000000;

struct EvalWeights eval_weights = {
	100,    /* stacks */
	 25,    /* moves */
	 20,    /* to_life */
	 20 };  /* to_enemy */

/* Minimum and summed distance to Dvonn stones. Used in eval_placing(). */
static int min_dist_to_dvonn[N];
static int tot_dist_to_dvonn[N];

int eval_dvonn_spread(const Board *board)
{
	int f, d, df[D], nd = 0, res = 0;
	for (f = 0; f < N; ++f) {
		if (board->fields[f].dvonns) df[nd++] = f;
	}
	for (f = 0; f < N; ++f) {
		int min_dist = -1;
		for (d = 0; d < nd; ++d) {
			int dist = distance(f, df[d]);
			if (min_dist == -1 || dist < min_dist) min_dist = dist;
		}
		res += min_dist * min_dist;
	}
	return res;
}

void eval_update_dvonns(const Board *board)
{
	int f, g, dist;

	for (f = 0; f < N; ++f) {
		min_dist_to_dvonn[f] = N;
		tot_dist_to_dvonn[f] = 0;
	}

	for (f = 0; f < N; ++f) {
		if (board->fields[f].dvonns > 0) {
			for (g = 0; g < N; ++g) {
				dist = distance(f, g);
				tot_dist_to_dvonn[g] += dist;
				if (dist < min_dist_to_dvonn[g]) {
					min_dist_to_dvonn[g] = dist;
				}
			}
		}
	}
}

/* Evaluate the board during the placing phase. */
val_t eval_placing(const Board *board)
{
	int n, player = next_player(board);
	val_t score[2] = { 0, 0 };
	int edge_pieces[2] = { 0, 0 };

	/* Scan board for player's stones and value them: */
	for (n = 0; n < N; ++n) {
		const Field *f = &board->fields[n];

		if (f->pieces > 0 && f->player >= 0) {
			/* Count how many neighboring fields exist that are not occupied
				by a friendly piece: */
			const int *step;
			int neighbours = 0;
			for (step = board_steps[1][n]; *step; ++step) {
				if (f[*step].player != f->player) ++neighbours;
			}
			if (min_dist_to_dvonn[n] == 1) score[f->player] += 10;
			if (board_neighbours[n] != (1<<6) - 1) edge_pieces[f->player] += 1;
			score[f->player] -= tot_dist_to_dvonn[n];
			if (neighbours < 2) score[f->player] -= 5*(2 - neighbours);
		}
	}

	/* Add penalty when one player occupies too few edge fields: */
	if (edge_pieces[1] - edge_pieces[0] > 3) {
		score[0] -= edge_pieces[1] - edge_pieces[0] - 3;
	}
	if (edge_pieces[0] - edge_pieces[1] > 3) {
		score[1] -= edge_pieces[0] - edge_pieces[1] - 3;
	}

	return score[player] - score[1 - player];
}

/* Evaluate a board during the stacking phase. */
val_t eval_stacking(const Board *board, bool *exact)
{
	int n, sign;
	const int *step;
	const Field *f, *g;
	int player = next_player(board);
	bool game_over = true;
	int stacks = 0, score = 0, moves = 0, to_life = 0, to_enemy = 0;

	for (n = 0; n < N; ++n) {
		f = &board->fields[n];
		if (f->removed || f->player < 0) continue;
		if (f->player == player) {
			sign = +1;
			++stacks;
			score += f->pieces;
		} else {
			sign = -1;
			--stacks;
			score -= f->pieces;
		}
		for (step = board_steps[f->pieces][n]; *step; ++step) {
			g = f + *step;
			if (g->removed) continue;
			if (f->mobile) {
				game_over = false;
				if (g->dvonns) to_life += sign;
				if ((f->player ^ g->player) > 0) to_enemy += sign;
			}
			moves += sign;
		}
	}

	if (game_over) return val_big*score;

	*exact = false;

	return stacks   * eval_weights.stacks
	     + moves    * eval_weights.moves
	     + to_life  * eval_weights.to_life
	     + to_enemy * eval_weights.to_enemy;
}
