#include "Eval.h"
#include <math.h>
#include <assert.h>

const val_t val_min = -1000000000;
const val_t val_max = +1000000000;
const val_t val_eps =           1;
const val_t val_big =     1000000;

struct EvalWeights eval_weights = {
	100,    /* stacks */
	  2,    /* score */
	 30,    /* moves */
	 20,    /* to_life */
	 20 };  /* to_enemy */

/* Minimum and summed distance to Dvonn stones. Used in eval_placing(). */
static int min_dist_to_dvonn[H][W];
static int tot_dist_to_dvonn[H][W];

int eval_dvonn_spread(const Board *board)
{
	int r, c, d, dr[D], dc[D], nd = 0, res = 0;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			if (board->fields[r][c].dvonns) {
				assert(nd < D);
				dr[nd] = r;
				dc[nd] = c;
				++nd;
			}
		}
	}
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			if (!board->fields[r][c].removed) {
				int min_dist = -1;
				for (d = 0; d < nd; ++d) {
					int dist = distance(r, c, dr[d], dc[d]);
					if (min_dist == -1 || dist < min_dist) min_dist = dist;
				}
				res += min_dist * min_dist;
			}
		}
	}
	return res;
}

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

void eval_update_dvonns(const Board *board)
{
	int r1, c1, r2, c2, dist;

	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			min_dist_to_dvonn[r1][c1] = H+W;
			tot_dist_to_dvonn[r1][c1] = 0;
		}
	}

	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			if (board->fields[r1][c1].dvonns > 0) {
				for (r2 = 0; r2 < H; ++r2) {
					for (c2 = 0; c2 < W; ++c2) {
						dist = distance(r1, c1, r2, c2);
						if (!board->fields[r2][c2].removed) {
							tot_dist_to_dvonn[r2][c2] += dist;
							if (dist < min_dist_to_dvonn[r2][c2]) {
								min_dist_to_dvonn[r2][c2] = dist;
							}
						}
					}
				}
			}
		}
	}
}

/* Evaluate the board during the placing phase. */
val_t eval_placing(const Board *board)
{
	int r, c, d, player = next_player(board);;
	val_t score[2] = { 0, 0 };
	int edge_pieces[2] = { 0, 0 };

	/* Scan board for player's stones and value them: */
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			const Field *f = &board->fields[r][c];

			if (f->pieces > 0 && f->player >= 0) {
				int neighbours = 0;

				/* Count how many neighboring fields exist that are not occupied
				   by a friendly piece: */
				for (d = 0; d < 6; ++d) {
					int r2 = r + DR[d], c2 = c + DC[d];
					if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W) {
						const Field *g = &board->fields[r2][c2];
						if (!g->removed && g->player != f->player) ++neighbours;
					}
				}

				if (min_dist_to_dvonn[r][c] == 1) score[f->player] += 10;
				if (is_edge_field(board,r, c)) edge_pieces[f->player] += 1;
				score[f->player] -= tot_dist_to_dvonn[r][c];
				if (neighbours < 2) score[f->player] -= 5*(2 - neighbours);
			}
		}
	}

	/* Add penalty when one player occupied too few edge fields: */
	if (edge_pieces[1] - edge_pieces[0] > 5) {
		score[0] -= edge_pieces[1] - edge_pieces[0] - 5;
	}
	if (edge_pieces[1] - edge_pieces[0] > 5) {
		score[0] -= edge_pieces[1] - edge_pieces[0] - 5;
	}

	return score[player] - score[1 - player];
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

	if (game_over) return val_big*score;

	return stacks   * eval_weights.stacks
	     + score    * eval_weights.score
	     + moves    * eval_weights.moves
	     + to_life  * eval_weights.to_life
	     + to_enemy * eval_weights.to_enemy;
}
