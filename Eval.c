#include "Eval.h"
#include <assert.h>

const val_t min_val = -9999;
const val_t max_val = +9999;

int eval_count_placing  = 0;
int eval_count_stacking = 0;
int eval_count_end      = 0;

/* Returns whether the given field lies on the edge of the board: */
static bool is_edge_field(const Board *board, int r, int c)
{
	int d;

	if (r == 0 || r == H - 1 || c == 0 || c == W - 1) return true;
	for (d = 0; d < 6; ++d) {
		if (board->fields[r + DR[d]][c + DC[d]].removed == (unsigned char)-1) {
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

	++eval_count_placing;

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
				if (is_edge_field(board,r, c)) score[f->player] += 5;
				score[f->player] -= tot_dist_to_dvonn;
				if (neighbours < 2) score[f->player] -= 5*(2 - neighbours);
			}
		}
	}
	p = next_player(board);
	return score[p] - score[1 - p];
}

/* Evaluate an end position. */
EXTERN val_t eval_end(const Board *board)
{
	++eval_count_end;
	return 100*board_score(board);
}

/* Evaluate a board during the stacking phase. */
EXTERN val_t eval_stacking(const Board *board)
{
	Move moves[2*M];
	int nmove, n, p /* , r, c */;
	val_t score[2] = { 0, 0 };

	assert(board->moves >= N);

	/* Check if this is an end position instead: */
	nmove = generate_all_moves(board, moves);
	if (nmove == 0) return eval_end(board);
	++eval_count_stacking;

	/* Value moves: */
	for (n = 0; n < nmove; ++n) {
		const Field *f = &board->fields[moves[n].r1][moves[n].c1];
		const Field *g = &board->fields[moves[n].r2][moves[n].c2];

		if (g->player == f->player) score[f->player] += 3;  /* to self */
		else if (g->player == NONE) score[f->player] += 4;  /* to Dvonn */
		else                        score[f->player] += 5;  /* to opponent */
	}
#if 0
	/* Value material: */
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			const Field *f = &board->fields[r][c];

			if (f->player >= 0) score[f->player] += 1;
		}
	}
#endif
	p = next_player(board);
	return score[p] - score[1 - p];
}

EXTERN val_t eval_intermediate(const Board *board)
{
	if (board->moves > N) {
		return eval_stacking(board);
	} else if (board->moves > D) {
		return eval_placing(board);
	} else {  /* board->moves <= D */
		return 0;
	}
}
