#include "AI.h"
#include <assert.h>
#include <stdio.h>

static const int inf = 999999999;

static int num_evaluated;

EXTERN bool ai_select_place(Board *board, Place *place)
{
	Place places[N];
	int n, nplace = generate_places(board, places);
	int best_val = -1, best_cnt = 0, player = board->moves%2;

	for (n = 0; n < nplace; ++n) {
		int val = 0;
		if (places[n].r == H/2 && places[n].c == W/2) {
			/* Play in center is preferred over random. */
			val += 1;
		} else {
			const Field *f =
				&board->fields[H - 1 - places[n].r][W - 1 - places[n].c];
			if (f->pieces != 0) {
				/* Play symmetric to an existing piece is preferred. */
				val += 2;
				if (board->moves < 3 ? f->dvonns : f->player == 1 - player) {
					/* play Dvonn opposite Dvonn, or white opposite black: */
					val += 4;
				}
			}
		}
		if (val > best_val) {
			best_val = val;
			best_cnt = 0;
		}
		if (val == best_val && rand()%++best_cnt == 0) *place = places[n];
	}
	return best_cnt > 0;
}

static int eval_end(const Board *board)
{
	return 100*board_score(board);
}

static int eval_intermediate(const Board *board)
{
	Move moves[2*M];
	int nmove, n, r, c, p;
	int score[2] = { 0, 0 };

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

	/* Value material: */
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			const Field *f = &board->fields[r][c];

			if (!f->removed && f->player != NONE) {
				score[f->player] += 1;
			}
		}
	}

	++num_evaluated;
	p = next_player(board);
	return score[p] - score[1 - p];
}

static int dfs(
	Board *board, int depth, int pass, int lo, int hi, Move *best )
{
	Move moves[M];
	int n, nmove;

	if (pass == 2) return eval_end(board);
	if (depth == 0) return eval_intermediate(board);

	nmove = generate_moves(board, moves);
	assert(nmove > 0);
	for (n = 0; n < nmove; ++n) {
		int val;
		Color old_player;

		board_move(board, &moves[n], &old_player);

		val = -dfs( board, nmove > 1 ? depth - 1 : depth,
			move_is_pass(&moves[n]) ? pass + 1 : 0, -hi, -lo, NULL );

		board_unmove(board, &moves[n], old_player);

		if (val > lo) {
			lo = val;
			if (best != NULL) *best = moves[n];
		}
		if (lo >= hi) break;
	}
	return lo;
}

EXTERN bool ai_select_move(Board *board, Move *move)
{
	int val;

	num_evaluated = 0;
	val = dfs(board, 4, 0, -inf, +inf, move);
	fprintf(stderr, "Value: %d (%d position evaluated)\n", val, num_evaluated);
	return true;
}
