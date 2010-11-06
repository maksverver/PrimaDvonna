#include "Eval.h"
#include <assert.h>

const val_t min_val = -9999;
const val_t max_val = +9999;

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
EXTERN val_t eval_placing(const Board *board)
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
				int min_dist_to_dvonn = max_val;
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

struct Weights {
	double Towers[2][2];        /* offset   0 */
	double Moves[2][2];         /* offset  32 */
	double Score[2][2];         /* offset  64 */
	double MovesToLife[2];      /* offset  96 */
	double MovesToEnemy[2];     /* offset 112 */
};

static struct Weights weights = {
	 /*   immobile          mobile      */
	 /* nolife life       nolife life   */
	{ { 0.000, 0.050 }, { 0.050, 0.050 } },  /* Towers */
	{ { 0.150, 0.000 }, { 0.200, 0.075 } },  /* Moves */
	{ { 0.005, 0.050 }, { 0.010, 0.075 } },  /* Score */

	{ 0.030, 0.200 },                        /* MovesToLife */
	{ 0.000, 0.050 } };                      /* MovesToEnemy */

/* Evaluate a board during the stacking phase. */
EXTERN val_t eval_stacking(const Board *board)
{
	int r1, c1, r2, c2, dir, sign, i, j;
	const Field *f, *g;
	val_t value;
	bool game_over = true;
	int wTowers [2][2] = { { 0, 0 }, { 0, 0 } };
	int wMoves  [2][2] = { { 0, 0 }, { 0, 0 } };
	int wScore  [2][2] = { { 0, 0 }, { 0, 0 } };
	int wMovesToLife  [2] = { 0, 0 };
	int wMovesToEnemy [2] = { 0, 0 };

	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			f = &board->fields[r1][c1];
			if (f->removed || f->player < 0) continue;
			sign = (f->player == WHITE) ? +1 : -1;
			wTowers[f->mobile?1:0][f->dvonns?1:0] += sign;
			wScore[f->mobile?1:0][f->dvonns?1:0] += f->pieces * sign;
			for (dir = 0; dir < 6; ++dir) {
				r2 = r1 + DR[dir] * f->pieces;
				c2 = c1 + DC[dir] * f->pieces;
				if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W) {
					g = &board->fields[r2][c2];
					if (g->removed) continue;
					if (f->mobile) game_over = false;
					if (g->dvonns) {
						wMovesToLife[f->mobile?1:0] += sign;
					}
					if (g->player == 1 - f->player) {
						wMovesToEnemy[f->mobile?1:0] += sign;
					}
					wMoves[f->mobile?1:0][f->dvonns?1:0] += sign;
				}
			}
		}
	}

	if (game_over) return eval_end(board);

	value = 0;
	for (i = 0; i < 2; ++i) {
		for (j = 0; j < 2; ++j) {
			value += wTowers [i][j] * weights.Towers [i][j];
			value += wMoves  [i][j] * weights.Moves  [i][j];
			value += wScore  [i][j] * weights.Score  [i][j];
		}
		value += wMovesToLife  [i] * weights.MovesToLife  [i];
		value += wMovesToEnemy [i] * weights.MovesToEnemy [i];
	}

	return (next_player(board) == WHITE) ? value : -value;
}

/* Evaluate an end position. */
EXTERN val_t eval_end(const Board *board)
{
	return 100*board_score(board);
}
