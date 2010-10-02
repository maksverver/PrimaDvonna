#include "AI.h"
#include "TT.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Minimum/maximum game values: */
const val_t min_val = -9999;
const val_t max_val = +9999;

/* Maximum search depth: */
static const int max_depth = 2*N;

/* Indicates whether to use the transposition-table: */
static bool g_use_tt = true;

static int num_evaluated;  /* DEBUG */

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

/* Counts how many neighbouring fields are controlled by the given neighbour: */
static bool count_neighbours(const Board *board, int r1, int c1, Color player)
{
	int d, r2, c2, res = 0;

	for (d = 0; d < 6; ++d) {
		r2 = r1 + DR[d];
		c2 = c1 + DC[d];
		if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W) {
			if (board->fields[r2][c2].player == player) ++res;
		}
	}
	return res;
}

/* Evaluate the board during the placing phase. */
static val_t eval_placing(const Board *board)
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
			int distance_to_dvonn = max_val;

			for (d = 0; d < D; ++d) {
				int dist = distance(r, c, dvonn_r[d], dvonn_c[d]);
				if (dist < distance_to_dvonn) distance_to_dvonn = dist;
			}

			if (f->pieces > 0 && f->player >= 0) {
				score[f->player] +=
					+3*is_edge_field(board, r, c)
					-2*(distance_to_dvonn > 1 ? distance_to_dvonn : -3)
					-1*count_neighbours(board, r, c, f->player);
			}
		}
	}
	p = next_player(board);
	return score[p] - score[1 - p];
}

/* Evaluate an end position. */
static val_t eval_end(const Board *board)
{
	return 100*board_score(board);
}

/* Evaluate a board during the stacking phase. */
static val_t eval_stacking(const Board *board)
{
	Move moves[2*M];
	int nmove, n, p;
	val_t score[2] = { 0, 0 };

	assert(board->moves >= N);

	nmove = generate_all_moves(board, moves);
	if (nmove == 0) return eval_end(board);

	/* Value moves: */
	for (n = 0; n < nmove; ++n) {
		const Field *f = &board->fields[moves[n].r1][moves[n].c1];
		const Field *g = &board->fields[moves[n].r2][moves[n].c2];

		if (g->player == f->player) score[f->player] += 3;  /* to self */
		else if (g->player == NONE) score[f->player] += 4;  /* to Dvonn */
		else                        score[f->player] += 5;  /* to opponent */
	}

	p = next_player(board);
	return score[p] - score[1 - p];
}

static val_t eval_intermediate(const Board *board)
{
	++num_evaluated;
	if (board->moves > N) {
		return eval_stacking(board);
	} else if (board->moves > D) {
		return eval_placing(board);
	} else {  /* board->moves <= D */
		return 0;
	}
}

static val_t dfs(Board *board, int depth, int pass, int lo, int hi, Move *best)
{
	val_t res = min_val;

	if (g_use_tt) { /* look up in transposition table: */
		val_t tt_lo, tt_hi;

		if (tt_fetch(board, depth, &tt_lo, &tt_hi)) {
			if (tt_lo == tt_hi) return tt_lo;
			assert((tt_lo == min_val) ^ (tt_hi == max_val));
			if (tt_lo >= hi) return tt_lo;
			if (tt_hi <= lo) return tt_hi;
			res = tt_lo;
		}
	}

	if (pass == 2 || depth == 0) {  /* evaluate leaf node */
		res = (pass == 2) ? eval_end(board) : eval_intermediate(board);
		if (g_use_tt) tt_store(board, max_depth, res, res);
		return res;
	} else {  /* evaluate interior node */
		Move moves[M];
		int n, nmove = generate_moves(board, moves), nbest = 0;
		val_t val;

		/* TODO: optional move ordering? */

		assert(nmove > 0);
		for (n = 0; n < nmove; ++n) {
			/* Evaluate position after n'th move: */
			board_do(board, &moves[n]);
			val = -dfs( board, nmove > 1 ? depth - 1 : depth,
				move_passes(&moves[n]) ? pass + 1 : 0, -hi, -res, NULL );
			board_undo(board, &moves[n]);

			/* Update value bounds: */
			if (best != NULL && val >= res) {  /* update best move */
				assert(lo == min_val);
				if (val > res) nbest = 0;
				if (rand()%++nbest == 0) *best = moves[n];
			}
			if (val > res) res = val;
			if (res >= hi) break;
		}
		if (g_use_tt) {
			tt_store(board, depth, res>lo?res:min_val, res<hi?res:max_val);
		}
		return res;
	}
	return lo;
}

EXTERN bool ai_select_move(Board *board, Move *move)
{
	int depth, nmove, tmove;
	val_t val;
	Move moves[M];

	/* Check if we have any moves to make: */
	nmove = generate_moves(board, moves);
	if (nmove == 0) {
		fprintf(stderr, "no moves available!\n");
		return false;
	}
	if (nmove == 1) {
		fprintf(stderr, "one move available!\n");
		*move = moves[0];
		return true;
	}

	tmove = generate_all_moves(board, NULL);
	if (board->moves < N) {
		depth = 4;
	} else {
		/* Select search depth based on total moves available: */
		if (tmove < 10) depth = 6;
		else if (tmove < 20) depth = 5;
		else if (tmove < 40) depth = 4;
		else depth = 3;
	}

	num_evaluated = 0;
	val = dfs(board, depth, 0, -max_val, +max_val, move);
	fprintf(stderr, "nmove=%d tmove=%d depth=%d val=%d num_evaluated=%'d\n",
		nmove, tmove, depth, val, num_evaluated);
	return true;
}
