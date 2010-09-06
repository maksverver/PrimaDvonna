#include <assert.h>
#include <string.h>
#include "Game.h"

Move move_pass = { 0, 0, 0, 0 };

static int dr[6] = { -1, -1,  0,  0, +1, +1 }; 
static int dc[6] = { -1,  0, -1, +1,  0, +1 };

static int max(int i, int j) { return i > j ? i : j; }

static int distance(int r1, int c1, int r2, int c2)
{
	int dx = c2 - c1;
	int dy = r2 - r1;
	int dz = dx - dy;
	return max(max(abs(dx), abs(dy)), abs(dz));
}

void board_clear(Board *board)
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

void board_place(Board *board, const Place *p)
{
	Field *f = &board->fields[p->r][p->c];
	f->pieces = 1;
	if (board->moves < D) {
		f->dvonns = 1;
	} else {
		f->player = board->moves & 1;
	}
	++board->moves;
}

void board_unplace(Board *board, const Place *p)
{
	Field *f = &board->fields[p->r][p->c];
	f->player = NONE;
	f->pieces = 0;
	f->dvonns = 0;
	--board->moves;
}

static bool reachable[H][W];

static void mark_reachable(Board *board, int r1, int c1) {
	int d, r2, c2;

	reachable[r1][c1] = true;
	for (d = 0; d < 6; ++d) {
		r2 = r1 + dr[d];
		c2 = c1 + dc[d];
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

void board_move(Board *board, const Move *m, Color *old_player)
{
	if (!move_is_pass(m)) {
		Field *f = &board->fields[m->r1][m->c1];
		Field *g = &board->fields[m->r2][m->c2];
		if (old_player) *old_player = g->player;
		g->player = f->player;
		g->pieces += f->pieces;
		g->dvonns += f->dvonns;
		f->removed = board->moves;
		/* TODO: elide call to remove_unreachable if the current field contains
			no dvonn piece itself AND (i have just one neighbour OR all
			neighbours are adjacent to each other). even if we must check,
			we only need to flood-fill starting from the neighbours (but
			we still need to clear the whole reachable array) */
		remove_unreachable(board);
	}
	++board->moves;
}

static void restore_unreachable(Board *board, int r1, int c1)
{
	int d, r2, c2;

	board->fields[r1][c1].removed = 0;
	for (d = 0; d < 6; ++d) {
		r2 = r1 + dr[d];
		c2 = c1 + dc[d];
		if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W &&
			board->fields[r2][c2].removed == board->moves) {
			restore_unreachable(board, r2, c2);
		}
	}
}

void board_unmove(Board *board, const Move *m, Color old_player)
{
	--board->moves;
	if (!move_is_pass(m)) {
		Field *f = &board->fields[m->r1][m->c1];
		Field *g = &board->fields[m->r2][m->c2];
		g->player = old_player;
		g->pieces -= f->pieces;
		g->dvonns -= f->dvonns;
		assert(f->removed == board->moves);
		restore_unreachable(board, m->r1, m->c1);
	}
}

void board_validate(const Board *board)
{
	int r, c;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			const Field *f = &board->fields[r][c];
			assert(f->pieces <= N);
			assert(f->dvonns <= D);
			assert(f->dvonns <= f->pieces);
			assert(f->dvonns == f->pieces ? f->player == NONE :
				f->player == WHITE || f->player == BLACK);
			if (f->removed == (unsigned char)-1) continue;
			if (board->moves < N) {
				assert(f->pieces == 0 || f->pieces == 1);
				assert(f->removed == 0);
			} else {
				assert(f->pieces > 0);
				assert(f->removed < board->moves);
			}
		}
	}
}

int generate_places(const Board *board, Place places[N])
{
	int r, c, n = 0;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			if (!board->fields[r][c].pieces && !board->fields[r][c].removed) {
				places[n].r = r;
				places[n].c = c;
				++n;
			}
		}
	}
	return n;
}

static bool mobile(const Board *board, int r, int c)
{
	int d;

	if (r == 0 || r == H - 1 || c == 0 || c == W - 1) return true;
	for (d = 0; d < 6; ++d) {
		if (board->fields[r + dr[d]][c + dc[d]].removed) return true;
	}
	return false;
}

int generate_moves(const Board *board, Move moves[M])
{
	const Field *f, *g;
	int r1, c1, d, r2, c2, n = 0;
	Color player = (board->moves - N)&1;

	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			f = &board->fields[r1][c1];
			if (!f->removed && mobile(board, r1, c1) && f->player == player) {
				for (d = 0; d < 6; ++d) {
					r2 = r1 + f->pieces*dr[d];
					c2 = c1 + f->pieces*dc[d];
					if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W) {
						g = &board->fields[r2][c2];
						if (!g->removed) {
							moves[n].r1 = r1;
							moves[n].c1 = c1;
							moves[n].r2 = r2;
							moves[n].c2 = c2;
							++n;
						}
					}
				}
			}
		}
	}
	if (n == 0) moves[n++] = move_pass;
	return n;
}
