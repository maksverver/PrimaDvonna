#include "Eval.h"
#include <math.h>
#include <assert.h>

const val_t min_val = -9999;
const val_t max_val = +9999;

EvalWeights weights = {
	0.10,  /* stack_base */
	3.00,  /* move_base */
	2.00,  /* move_opponent */
	0.30,  /* move_near_dvonn_mult */
	0.50,  /* move_near_dvonn_decay */
	0.25,  /* move_immobile */
	0.60,  /* stack_immobile */
	0.20,  /* pieces_mult */
	0.90   /* pieces_decay */ };

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
	assert(d == D);

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

/* Evaluate a board during the stacking phase. */
EXTERN val_t eval_stacking(const Board *board)
{
	int dvonn_dist[H][W];
	int dvonn_r[D], dvonn_c[D], ndvonn;
	int r1, c1, r2, c2, n, dist, dir;
	val_t player_vals[2] = { 0.0f, 0.0f };
	bool game_over;

	/* Find Dvonn stones: */
	ndvonn = 0;
	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			const Field *f = &board->fields[r1][c1];
			if (!f->removed && f->dvonns) {
				dvonn_r[ndvonn] = r1;
				dvonn_c[ndvonn] = c1;
				++ndvonn;
			}
		}
	}

	/* Compute distance to Dvonn stones: */
	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			dvonn_dist[r1][c1] = -1;
			if (!board->fields[r1][c1].removed) {
				for (n = 0; n < ndvonn; ++n) {
					dist = distance(r1, c1, dvonn_r[n], dvonn_c[n]);
					if (dvonn_dist[r1][c1] == -1 || dist < dvonn_dist[r1][c1]) {
						dvonn_dist[r1][c1] = dist;
					}
				}
			}
		}
	}

	/* Value fields: */
	game_over = true;
	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			const Field *f = &board->fields[r1][c1];
			if (!f->removed && f->player >= 0) {
				float field_val = weights.stack_base;

				for (dir = 0; dir < 6; ++dir) {
					r2 = r1 + DR[dir]*f->pieces;
					c2 = c1 + DC[dir]*f->pieces;
					if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W) {
						/* Value of a move: */
						const Field *g = &board->fields[r2][c2];
						if (!g->removed) {
							if (f->mobile) game_over = false;

							/* Move base value: */
							float move_val = weights.move_base;

							/* Move to opponent: */
							if ((g->player ^ f->player) > 0) {
								move_val += weights.move_opponent;
							}

							/* Proximity to Dvonns: */
							move_val += weights.move_near_dvonn_mult *
								pow( weights.move_near_dvonn_base,
								     dvonn_dist[r2][c2] );

							/* Penalty for immobility: */
							if (!g->mobile) move_val *= weights.move_immobile;
							field_val += move_val;
						}
					}
				}

				/* Penalty if immobile */
				if (!f->mobile) field_val *= weights.stack_immobile;

				/* Value of pieces: (independent of mobility) */
				field_val += f->pieces * weights.pieces_mult *
					powf(weights.pieces_base, dvonn_dist[r1][c1]);

				player_vals[f->player] += field_val;
			}
		}
	}

	if (game_over) return eval_end(board);

	n = next_player(board);
	return player_vals[n] - player_vals[1 - n];
}


/* Evaluate an end position. */
EXTERN val_t eval_end(const Board *board)
{
	return 100*board_score(board);
}
