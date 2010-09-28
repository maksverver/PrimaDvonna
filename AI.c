#include "AI.h"
#include "TT.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const int inf = 999999999;

static int num_evaluated;

/* Calculates the sum of the distance of each Dvonn stone to the given field: */
static int distance_to_dvonns(Board *board, int r1, int c1)
{
	int r2, c2, res = 0;

	for (r2 = 0; r2 < H; ++r2) {
		for (c2 = 0; c2 < W; ++c2) {
			const Field *f = &board->fields[r2][c2];
			if (f->dvonns && !f->removed) res += distance(r1, c1, r2, c2);
		}
	}
	return res;
}

/* Returns whether the given field lies on the edge of the board: */
EXTERN bool is_edge_field(const Board *board, int r, int c)
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
EXTERN bool count_neighbours(const Board *board, int r1, int c1, Color player)
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

EXTERN bool select_place(Board *board, Move *move)
{
	move->r2 = move->c2 = -1;

	if (board->moves%2 == 0) {  /* I play white. Fill board symmetrically: */
		if (board->moves == 0) {
			move->r1 = H/2;
			move->c1 = W/2;
			return true;
		} else {
			int r1, c1, r2, c2;

			for (r1 = 0; r1 < H; ++r1) {
				for (c1 = 0; c1 < W; ++c1) {
					r2 = H - 1 - r1;
					c2 = W - 1 - c1;
					if (board->fields[r1][c1].pieces &&
						!board->fields[r2][c2].pieces) {
						move->r1 = r2;
						move->c1 = c2;
						return true;
					}
				}
			}
		}
	} else {  /* I play black. */
		Move moves[N];
		int nmove = generate_moves(board, moves);

		assert(nmove > 0);
		if (board->moves < 3) {

			/* Place Dvonn stone randomly. */
			*move = moves[rand()%nmove];
			return true;

		} else {
			int r, c, n, val, best_val = 0, best_cnt = 0;

			/* Place random stone close to Dvonn stones and board's edge: */
			for (n = 0; n < nmove; ++n) {
				r = moves[n].r1;
				c = moves[n].c1;
				val = 3*is_edge_field(board, r, c)
					- distance_to_dvonns(board, r, c)
					- count_neighbours(board, r, c, next_player(board));
				if (best_cnt == 0 || val > best_val) {
					best_cnt = 0;
					best_val = val;
				}
				if (val == best_val && rand()%++best_cnt == 0) {
					*move = moves[n];
				}
			}
			assert(best_cnt > 0);
			return true;
		}
	}
	return false;  /* should not get here. */
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

static int eval_end_tt(const Board *board)
{
	hash_t hash = hash_board(board);
	TTEntry *entry = &tt[hash%TT_SIZE];
	if (entry->hash != hash) {
		entry->hash  = hash;
		entry->value = eval_end(board);
	}
	return entry->value;
}

static int eval_intermediate_tt(const Board *board)
{
	unsigned char data[50];
	hash_t hash;
	TTEntry *entry;

	serialize_board(board, data);
	hash = fnv1(data, 50);
	entry = &tt[hash%TT_SIZE];
	if (entry->hash != hash) {
		entry->hash  = hash;
		entry->value = eval_intermediate(board);
	}
	return entry->value;
}

static int dfs(
	Board *board, int depth, int pass, int lo, int hi, Move *best )
{
	Move moves[M];
	int n, nmove;

	if (pass == 2) return eval_end_tt(board);
	if (depth == 0) return eval_intermediate_tt(board);

	nmove = generate_moves(board, moves);
	assert(nmove > 0);
	for (n = 0; n < nmove; ++n) {
		int val;
		Color old_player;

		board_do(board, &moves[n], &old_player);

		val = -dfs( board, nmove > 1 ? depth - 1 : depth,
			move_passes(&moves[n]) ? pass + 1 : 0, -hi, -lo, NULL );

		board_undo(board, &moves[n], old_player);

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
	int val, depth, num_moves;

	/* Placement phase is handled separately: */
	if (board->moves < N) return select_place(board, move);

	num_moves = generate_all_moves(board, NULL);
	if (num_moves < 10) depth = 6;
	else if (num_moves < 20) depth = 5;
	else if (num_moves < 40) depth = 4;
	else depth = 3;

	num_evaluated = 0;
	val = dfs(board, depth, 0, -inf, +inf, move);
	fprintf(stderr, "num_moves=%d depth=%d val=%d num_evaluated=%d\n",
		num_moves, depth, val, num_evaluated);
	return true;
}
