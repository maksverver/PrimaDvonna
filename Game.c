#include <assert.h>
#include <string.h>
#include "Game.h"

Move move_pass = { -1, -1, -1, -1 };

const int DR[6] = {  0, -1, -1,  0, +1, +1 };
const int DC[6] = { +1,  0, -1, -1,  0, +1 };

static int max(int i, int j) { return i > j ? i : j; }

EXTERN int distance(int r1, int c1, int r2, int c2)
{
	int dx = c2 - c1;
	int dy = r2 - r1;
	int dz = dx - dy;
	return max(max(abs(dx), abs(dy)), abs(dz));
}

EXTERN void board_clear(Board *board)
{
	int r, c;
	board->moves = 0;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			board->fields[r][c].player = NONE;
			board->fields[r][c].pieces = 0;
			board->fields[r][c].dvonns = 0;
			board->fields[r][c].removed =
				distance(r, c, H/2, W/2) <= W/2 ? 0 : -1;
		}
	}
}

static void place(Board *board, const Move *m)
{
	Field *f = &board->fields[m->r1][m->c1];
	f->pieces = 1;
	if (board->moves < D) {
		f->dvonns = 1;
	} else {
		f->player = board->moves & 1;
	}
}

static void unplace(Board *board, const Move *m)
{
	Field *f = &board->fields[m->r1][m->c1];
	f->player = NONE;
	f->pieces = 0;
	f->dvonns = 0;
}

/* Temporary space shared by mark_reachable() and remove_reachable(): */
static bool reachable[H][W];

static void mark_reachable(Board *board, int r1, int c1) {
	int d, r2, c2;

	reachable[r1][c1] = true;
	for (d = 0; d < 6; ++d) {
		r2 = r1 + DR[d];
		c2 = c1 + DC[d];
		if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W &&
			!board->fields[r2][c2].removed && !reachable[r2][c2]) {
			mark_reachable(board, r2, c2);
		}
	}
}

static void remove_unreachable(Board *board)
{
	int r, c;

	memset(reachable, 0, sizeof(reachable));
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			if (!board->fields[r][c].removed &&
				board->fields[r][c].dvonns && !reachable[r][c]) {
				mark_reachable(board, r, c);
			}
		}
	}
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			if (!board->fields[r][c].removed && !reachable[r][c]) {
				board->fields[r][c].removed = board->moves;
			}
		}
	}
}

/* Returns whether the field at (r,c) could be a bridge (a field that connects
   two or more otherwise unconnected segments of the board) by analyzing its
   neighbours: a field cannot be a bridge if all of its neighbours are adjacent
   to each other. */
static bool may_be_bridge(Board *board, int r, int c)
{
	static bool bridge_index[1<<6] = {
		0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0,
		0, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0,
		0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0,
		0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0 };

	int d, mask = 0;

	for (d = 0; d < 6; ++d) {
		int nr = r + DR[d], nc = c + DC[d];
		if (nr >= 0 && nr < H && nc >= 0 && nc < W &&
			!board->fields[nr][nc].removed) mask |= 1<<d;
	}
	return bridge_index[mask];
}

static void stack(Board *board, const Move *m)
{
	Color tmp_player;
	Field *f = &board->fields[m->r1][m->c1];
	Field *g = &board->fields[m->r2][m->c2];

	tmp_player = f->player;
	f->player = g->player;
	g->player = tmp_player;
	g->pieces += f->pieces;
	g->dvonns += f->dvonns;
	f->removed = board->moves;
	/* We must remove disconnected fields, but since remove_unreachable()
		is expensive, we try to elide it if permissible: */
	if (f->dvonns || may_be_bridge(board, m->r1, m->c1)) {
		remove_unreachable(board);
	}
}

static void restore_unreachable(Board *board, int r1, int c1)
{
	int d, r2, c2;

	board->fields[r1][c1].removed = 0;
	for (d = 0; d < 6; ++d) {
		r2 = r1 + DR[d];
		c2 = c1 + DC[d];
		if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W &&
			board->fields[r2][c2].removed == board->moves) {
			restore_unreachable(board, r2, c2);
		}
	}
}

static void unstack(Board *board, const Move *m)
{
	Color tmp_player;
	Field *f = &board->fields[m->r1][m->c1];
	Field *g = &board->fields[m->r2][m->c2];

	tmp_player = f->player;
	f->player = g->player;
	g->player = tmp_player;
	g->pieces -= f->pieces;
	g->dvonns -= f->dvonns;
	assert(f->removed == board->moves);
	restore_unreachable(board, m->r1, m->c1);
}

EXTERN void board_do(Board *board, const Move *m)
{
	if (m->r2 >= 0) {
		stack(board, m);
	} else if (m->r1 >= 0) {
		place(board, m);
	}
	++board->moves;
}

EXTERN void board_undo(Board *board, const Move *m)
{
	--board->moves;
	if (m->r2 >= 0) {  /* stack */
		unstack(board, m);
	} else if (m->r1 >= 0) {  /* place */
		unplace(board, m);
	}
}

EXTERN void board_validate(const Board *board)
{
	int r, c;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			const Field *f = &board->fields[r][c];
			assert(f->pieces <= N);
			assert(f->dvonns <= D);
			assert(f->dvonns <= f->pieces);
			if (f->dvonns == f->pieces || f->pieces == 0) {
				assert(f->player == NONE);
			}
			if (f->removed == -1) continue;
			if (board->moves < N) {
				assert(f->pieces == 0 || f->pieces == 1);
				assert(f->removed == 0);
			} else {
				assert(f->pieces > 0);
				assert(f->removed < board->moves);
			}
		}
	}
	/* This doesn't really belong here, but I want to check it somewhere: */
	assert(sizeof(Move) == sizeof(int));
}

static int gen_places(const Board *board, Move moves[N])
{
	int r, c, nmove = 0;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			if (!board->fields[r][c].pieces && !board->fields[r][c].removed) {
				if (moves) {
					Move new_move = { r, c, -1, -1 };
					moves[nmove] = new_move;
				}
				++nmove;
			}
		}
	}
	return nmove;
}

static bool mobile(const Board *board, int r, int c)
{
	int d;

	if (r == 0 || r == H - 1 || c == 0 || c == W - 1) return true;
	for (d = 0; d < 6; ++d) {
		if (board->fields[r + DR[d]][c + DC[d]].removed) return true;
	}
	return false;
}

static int gen_stacks(const Board *board, Move *moves, Color player)
{
	const Field *f, *g;
	int r1, c1, d, r2, c2, nmove = 0;

	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			f = &board->fields[r1][c1];
			if (!f->removed && f->player != NONE &&
				(player == NONE || player == f->player) &&
				mobile(board, r1, c1)) {
				for (d = 0; d < 6; ++d) {
					r2 = r1 + f->pieces*DR[d];
					c2 = c1 + f->pieces*DC[d];
					if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W) {
						g = &board->fields[r2][c2];
						if (!g->removed) {
							if (moves) {
								Move new_move = { r1, c1, r2, c2};
								moves[nmove] = new_move;
							}
							++nmove;
						}
					}
				}
			}
		}
	}
	if (player != NONE && nmove == 0) {
		moves[nmove++] = move_pass;
	}
	return nmove;
}

EXTERN int generate_all_moves(const Board *board, Move moves[2*M])
{
	return board->moves < N
		? gen_places(board, moves)
		: gen_stacks(board, moves, NONE);
}

EXTERN int generate_moves(const Board *board, Move moves[M])
{
	return board->moves < N
		? gen_places(board, moves)
		: gen_stacks(board, moves, next_player(board));
}

EXTERN void board_scores(const Board *board, int scores[2])
{
	int r, c;

	scores[0] = scores[1] = 0;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			const Field *f = &board->fields[r][c];

			if (!f->removed && (f->player == 0 || f->player == 1)) {
				scores[f->player] += f->pieces;
			}
		}
	}
}

EXTERN bool valid_move(const Board *board, const Move *move)
{
	Move moves[M];
	int i, nmove;

	nmove = generate_moves(board, moves);
	for (i = 0; i < nmove; ++i) {
		if (move_compare(&moves[i], move) == 0) return true;
	}
	return false;
}

EXTERN int board_score(const Board *board)
{
	int sc[2], player = next_player(board);
	board_scores(board, sc);
	return sc[player] - sc[1 - player];
}

/* A theoretical upper bound on the total number of moves is N + (N - 1),
   since it takes N moves to set up the board, and N - 1 moves to reduce all
   pieces to a single stack. This is the estimate used during the placement
   phase.

   However, in practice the number of moves is significantly lower. Therefore,
   during the stacking phase, we count how many stacks remain in the game, and
   use that to determine the maximum number of moves. This is still a gross
   over-estimation. */
EXTERN int max_moves_left(const Board *board)
{
	int r, c, stacks = 0;

	if (board->moves < N) {
		return N + (N - 1) - board->moves;
	}

	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			if (!board->fields[r][c].removed) ++stacks;
		}
	}
	return stacks - 1;
}
