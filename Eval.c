#include "Eval.h"
#include <math.h>
#include <assert.h>

#ifndef FIXED_PARAMS
/* TODO! add (3?) extra fields for distance-to-Dvonn weighting */
struct EvalWeights eval_weights = {
	EVAL_DEFAULT_WEIGHT_STACKS,
	EVAL_DEFAULT_WEIGHT_MOVES,
	EVAL_DEFAULT_WEIGHT_TO_LIFE,
	EVAL_DEFAULT_WEIGHT_TO_ENEMY,
	EVAL_DEFAULT_WEIGHT_FIELD_BASE,
	EVAL_DEFAULT_WEIGHT_FIELD_BONUS,
	EVAL_DEFAULT_WEIGHT_FIELD_SHIFT };
#endif

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

/* Evaluate the board during the placing phase. */
val_t eval_placing(const Board *board)
{
	static long long prev_dvonns = -1LL;
	static int min_dist_to_dvonn[N];
	static int tot_dist_to_dvonn[N];

	int n, player = next_player(board);
	val_t score[2] = { 0, 0 };
	int edge_pieces[2] = { 0, 0 };

	if (board->dvonns != prev_dvonns) {
		/* Must reculculate distance to Dvonn stones: */
		int m, dist;

		for (n = 0; n < N; ++n) {
			min_dist_to_dvonn[n] = N;
			tot_dist_to_dvonn[n] = 0;
		}

		for (n = 0; n < N; ++n) {
			if (board->fields[n].dvonns > 0) {
				for (m = 0; m < N; ++m) {
					dist = distance(n, m);
					tot_dist_to_dvonn[m] += dist;
					if (dist < min_dist_to_dvonn[m]) {
						min_dist_to_dvonn[m] = dist;
					}
				}
			}
		}
	}

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
	static val_t field_value[N];
	static long long prev_dvonns = -1LL;

	int n, m;
	const int *step;
	const Field *f, *g;
	int player = next_player(board);
	bool game_over = true;
	val_t score = 0, stacks = 0, moves = 0, to_life = 0, to_enemy = 0;

	if (board->dvonns != prev_dvonns) {
		/* Recalculate value of fields: */
		for (n = 0; n < N; ++n) field_value[n] = EVAL_WEIGHT_FIELD_BASE;
		for (n = 0; n < N; ++n) {
			if (board->fields[n].dvonns) {
				for (m = 0; m < N; ++m) {
					field_value[m] += EVAL_WEIGHT_FIELD_BONUS >>
						(EVAL_WEIGHT_FIELD_SHIFT*distance(n, m));
				}
			}
		}
		prev_dvonns = board->dvonns;
	}

#define EVAL_FIELD(X) \
	do {                                                                      \
		score  X f->pieces;                                                   \
		stacks X field_value[n];                                              \
		for (step = board_steps[f->pieces][n]; *step; ++step) {               \
			m = n + *step;                                                    \
			g = f + *step;                                                    \
			if (g->removed) continue;                                         \
			if (f->mobile) {                                                  \
				game_over = false;                                            \
				if (g->dvonns) to_life X field_value[m];                      \
				if ((f->player ^ g->player) > 0) to_enemy X field_value[m];   \
			}                                                                 \
			moves X field_value[m];                                           \
		}                                                                     \
	} while(0)                                                                \

	for (n = 0; n < N; ++n) {
		f = &board->fields[n];
		if (f->removed || f->player < 0) continue;
		if (player == f->player) EVAL_FIELD(+=); else EVAL_FIELD(-=);
	}

	if (game_over) return val_big*score;
	*exact = false;
	return stacks   * EVAL_WEIGHT_STACKS
	     + moves    * EVAL_WEIGHT_MOVES
	     + to_life  * EVAL_WEIGHT_TO_LIFE
	     + to_enemy * EVAL_WEIGHT_TO_ENEMY;

#undef EVAL_FIELD
}
