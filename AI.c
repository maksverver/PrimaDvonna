#include "AI.h"
#include <assert.h>
#include <stdio.h>

bool ai_select_place(Board *board, Place *place)
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
	Move moves[2][M];
	int nmove[2];

	generate_all_moves(board, moves[0], moves[1], &nmove[0], &nmove[1]);
	if (nmove[0] + nmove[1] == 0) return eval_end(board);
	return nmove[(board->moves - N)%2] - nmove[(board->moves - N + 1)%2];
}

static int dfs(Board *board, int depth, int pass, Move *best)
{
	Move moves[M];
	int n, nmove, res = -10000;

	if (pass == 2) return eval_end(board);
	if (depth == 0) return eval_intermediate(board);

	nmove = generate_moves(board, moves);
	assert(nmove > 0);
	if (move_is_pass(&moves[0])) {
		/* Handle pass separately, because it is always a single move, and thus
		   we don't decrease depth for it: */
		if (best != NULL) *best = moves[0];
		board_move(board, &moves[0], NULL);
		res = -dfs(board, depth, pass + 1, NULL);
		board_unmove(board, &moves[0], NONE);
		return res;
	}
	for (n = 0; n < nmove; ++n) {
		int val;
		Color old_player;

		board_move(board, &moves[n], &old_player);
		val = -dfs(board, depth - 1, 0, NULL);
		board_unmove(board, &moves[n], old_player);
		if (val > res) {
			res = val;
			if (best != NULL) *best = moves[n];
		}
	}
	return res;
}

bool ai_select_move(Board *board, Move *move)
{
	int val = dfs(board, 2, 0, move);
	fprintf(stderr, "Value: %d\n", val);
	return true;
}
