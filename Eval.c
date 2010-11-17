#include "Eval.h"
#include <math.h>
#include <assert.h>

const val_t val_min = -1000.0f * (N + 1);
const val_t val_max = +1000.0f * (N + 1);

struct EvalWeights eval_weights = {
	1.00f,    /* stacks */
	0.02f,    /* score */
	0.30f,    /* moves */
	0.20f,    /* to_life */
	0.20f };  /* to_enemy */

/* Returns whether the given field lies on the edge of the board: */
static bool is_edge_field(const Board *board, int r, int c)
{
	int d;

	if (r == 0 || r == H - 1 || c == 0 || c == W - 1) return true;
	for (d = 0; d < 6; ++d) {
		if (board->fields[r + DR[d]][c + DC[d]].removed < 0) {
			return true;
		}
	}
	return false;
}

/* Evaluate the board during the placing phase. */
val_t eval_placing(const Board *board)
{
	int r, c, p;
	val_t score[2] = { 0, 0 };
	int dvonn_r[D], dvonn_c[D], d = 0;

	/* Find location of Dvonn stones. */
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			if (board->fields[r][c].dvonns > 0) {
				dvonn_r[d] = r;
				dvonn_c[d] = c;
				++d;
			}
		}
	}
	/* Can't evaluate before all Dvonn stones are placed: */
	if (d < D) return 0;

	/* Scan board for player's stones and value them: */
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			const Field *f = &board->fields[r][c];

			if (f->pieces > 0 && f->player >= 0) {
				int min_dist_to_dvonn = val_max;
				int tot_dist_to_dvonn = 0;
				int neighbours = 0;

				/* Count distance to dvonns: */
				for (d = 0; d < D; ++d) {
					int dist = distance(r, c, dvonn_r[d], dvonn_c[d]);
					tot_dist_to_dvonn += dist;
					if (dist < min_dist_to_dvonn) min_dist_to_dvonn = dist;
				}

				/* Count how many neighboring fields exist that are not occupied
				   by a friendly piece: */
				for (d = 0; d < 6; ++d) {
					int r2 = r + DR[d], c2 = c + DC[d];
					if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W) {
						const Field *g = &board->fields[r2][c2];
						if (!g->removed && g->player != f->player) ++neighbours;
					}
				}

				if (min_dist_to_dvonn == 1) score[f->player] += 15;
				if (is_edge_field(board,r, c)) score[f->player] += 2;
				score[f->player] -= tot_dist_to_dvonn;
				if (neighbours < 2) score[f->player] -= 5*(2 - neighbours);
			}
		}
	}
	p = next_player(board);
	return score[p] - score[1 - p];
}

/* Evaluate a board during the stacking phase. */
val_t eval_stacking(const Board *board)
{
	int n, sign;
	const int *step;
	const Field *f, *g;
	int player = next_player(board);
	bool game_over = true;
	int stacks = 0, score = 0, moves = 0, to_life = 0, to_enemy = 0;

	for (n = 0; n < W*H; ++n) {
		f = &board->fields[0][n];
		if (f->removed || f->player < 0) continue;
		sign = (f->player == player) ? +1 : -1;
		stacks += sign;
		score  += f->pieces * sign;
		for (step = board_steps[f->pieces][0][n]; *step; ++step) {
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

	if (game_over) return 1000.0f*score;

	return stacks   * eval_weights.stacks
	     + score    * eval_weights.score
	     + moves    * eval_weights.moves
	     + to_life  * eval_weights.to_life
	     + to_enemy * eval_weights.to_enemy;
}

/* Evaluate an end position. */
val_t eval_end(const Board *board)
{
	return 1000.0f*board_score(board);
}
