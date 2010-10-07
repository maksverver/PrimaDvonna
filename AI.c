#include "AI.h"
#include "Time.h"
#include "TT.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Minimum/maximum game values: */
const val_t min_val = -9999;
const val_t max_val = +9999;

/* Maximum search depth: */
static const int max_depth = 20;

/* Search algorithm paramters: */
bool ai_use_tt = true;
bool ai_use_mo = false;

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

			if (f->pieces > 0 && f->player >= 0) {
				int min_dist_to_dvonn = max_val;
				int tot_dist_to_dvonn = 0;

				for (d = 0; d < D; ++d) {
					int dist = distance(r, c, dvonn_r[d], dvonn_c[d]);
					tot_dist_to_dvonn += dist;
					if (dist < min_dist_to_dvonn) min_dist_to_dvonn = dist;
				}

				if (min_dist_to_dvonn == 1) score[f->player] += 20;
				if (is_edge_field(board,r, c)) score[f->player] += 10;
				score[f->player] -= tot_dist_to_dvonn;
				score[f->player] -= count_neighbours(board, r, c, f->player);
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
	int nmove, n, p /* , r, c */;
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

/* Shuffles a list of moves. */
static void shuffle_moves(Move *moves, int n)
{
	while (n > 1) {
		int m = rand()%n--;
		Move tmp = moves[m];
		moves[m] = moves[n];
		moves[n] = tmp;
	}
}

static void sort_moves(Move *moves, val_t *values, int nmove)
{
	/* Insertion sort. FIXME: faster sorting algorithm? */
	int i, j;
	Move m;
	val_t v;

	for (i = 1; i < nmove; ++i) {
		m = moves[i];
		v = values[i];
		for (j = i; j > 0; --j) {
			if (values[j - 1] <= v) break;
			moves[j] = moves[j - 1];
			values[j] = values[j - 1];
		}
		moves[j] = m;
		values[j] = v;
	}
}

/* Implements depth-first minimax search with alpha-beta pruning.

   Takes the current game state in `board' and `pass' (the number of passes
   in succession) and the maximum search depth `depth', returns the value of
   the game position in the range [lo+1:hi-1], or if the value is <= lo, then
   it returns an upper bound on the game value, or if the value is >= hi, then
   it returns a lower bound on the value.

   If `best' is not NULL, then the best move is assigned to *best.

   Note that we try to return as tight an upper bound as possible without
   searching more nodes than stricly necessary. This means in particular that
   we try to return bounds which are unequal to lo and hi if possible. This
   is beneficial when re-using transposition table entries. */
static val_t dfs(Board *board, int depth, int pass, int lo, int hi, Move *best)
{
	hash_t hash = (hash_t)-1;
	TTEntry *entry = NULL;
	val_t res = min_val;

	if (ai_use_tt) { /* look up in transposition table: */
		hash = hash_board(board);
		entry = &tt[hash%TT_SIZE];
		if (entry->hash == hash && entry->depth >= depth) {
			if (best == NULL) {
				if (entry->lo == entry->hi) return entry->lo;
				if (entry->lo >= hi) return entry->lo;
				if (entry->hi <= lo) return entry->hi;
				res = entry->lo;
			} else {  /* best != NULL */
				if (entry->depth == depth) res = entry->lo;
			}
		}
	}
	if (pass == 2) {  /* evaluate end position */
		assert(best == NULL);
		res = eval_end(board);
		if (ai_use_tt) {
			entry->hash  = hash;
			entry->lo    = res;
			entry->hi    = res;
			entry->depth = max_depth;
		}
	} else if (depth == 0) {  /* evaluate intermediate position */
		assert(best == NULL);
		res = eval_intermediate(board);
		if (ai_use_tt) {
			/* FIXME: eval_intermediate() could end up calling eval_end() if 
			   the current position is actually an end position. In that case,
			   we should really store depth = max_depth here! */
			entry->hash  = hash;
			entry->lo    = res;
			entry->hi    = res;
			entry->depth = 0;
		}
	} else {  /* evaluate interior node */
		Move moves[M];
		int n, nmove = generate_moves(board, moves);
		int next_lo = -hi;
		int next_hi = (-res < -lo) ? -res : -lo;

		if (nmove == 1) {
			/* Only one move available: */
			if (best != NULL) *best = moves[0];
			board_do(board, &moves[0]);
			res = -dfs(board, depth, (move_passes(&moves[0]) ? pass+1 : 0),
					   next_lo, next_hi, NULL);
			board_undo(board, &moves[0]);
		} else {
			/* Multiple moves available */
			assert(nmove > 1);
			if (best != NULL) {
				*best = move_pass;
				shuffle_moves(moves, nmove);
			}
			if (ai_use_mo && depth > 2) {
				val_t values[M];
				for (n = 0; n < nmove; ++n) {
					board_do(board, &moves[n]);
					values[n] = -dfs(board, depth-3, 0, next_lo, next_hi, NULL);
					board_undo(board, &moves[n]);
				}
				sort_moves(moves, values, nmove);
			}
			for (n = 0; n < nmove; ++n) {
				val_t val;

				/* Evaluate position after n'th move: */
				board_do(board, &moves[n]);
				val = -dfs(board, depth - 1, 0, next_lo, next_hi, NULL);
				board_undo(board, &moves[n]);

				/* Update value bounds: */
				if (val > res) {
					res = val;
					if (-res < next_hi) next_hi = -res;
					if (best != NULL) *best = moves[n];
				}
				if (res >= hi) break;
			}
			if (ai_use_tt) {
				entry->hash  = hash;
				entry->lo    = res > lo ? res : min_val;
				entry->hi    = res < hi ? res : max_val;
				entry->depth = depth;
			}
			assert(best == NULL || !move_passes(best));
		}
	}
	return res;
}

EXTERN bool ai_select_move(Board *board, Move *move)
{
	val_t val;
	Move moves[M];
	int nmove = generate_moves(board, moves);

	/* Check if we have any moves to make: */
	if (nmove == 0) {
		fprintf(stderr, "no moves available!\n");
		return false;
	}
	if (nmove == 1) {
		fprintf(stderr, "one move available!\n");
		*move = moves[0];
		return true;
	}

	if (board->moves < N) {
		/* Fixed-depth search during placement phase: */
		val = dfs(board, 2, 0, min_val, max_val, move);
		fprintf(stderr, "val=%d\n", val);
	} else {
		/* Increment max depth during placement phase: */
		static int depth = 2;
		int moves_left = max_moves_left(board);
		double start = time_used(), left = time_left();
		double used = 0, budget = left/(moves_left/2 + 1);

		fprintf(stderr, "[%.3fs] %.3fs left for %d moves; budget is %.3fs.\n",
			start, left, moves_left, budget);

		num_evaluated = 0;
		for (;;) {
			val = dfs(board, depth, 0, min_val, max_val, move);
			used = time_used() - start;
			fprintf(stderr, "[%.3fs] nmove=%d depth=%d val=%d num_evaluated=%'d"
				" (%.3fs used)\n", time_used(), nmove, depth, val,
				num_evaluated, used);
			if (used > budget) fprintf(stderr, "WARNING: over budget!\n");
			if (depth == max_depth || used > 0.25*left/moves_left) break;
			++depth;
			fprintf(stderr, "Increased search depth to %d.\n", depth);
		}
	}
	return true;
}

EXTERN bool ai_select_move_fixed(Board *board, Move *move, int depth)
{
	Move moves[M];
	int nmove = generate_moves(board, moves);
	val_t val;

	if (nmove == 0) {
		fprintf(stderr, "no moves available!\n");
		return false;
	}
	if (nmove == 1) {
		fprintf(stderr, "one move available!\n");
		*move = moves[0];
		return true;
	}
	num_evaluated = 0;
	val = dfs(board, depth, 0, min_val, max_val, move);
	fprintf(stderr, "depth=%d val=%d num_evaluated=%'d\n",
		depth, val, num_evaluated);
	return true;
}

EXTERN val_t ai_evaluate(const Board *board)
{
	return eval_intermediate(board);
}
