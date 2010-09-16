#include <assert.h>
#include <string.h>
#include "Game.h"

Move move_pass = { -1, -1, -1, -1 };

/* Directions. These are ordered in CCW order starting to the right. Order is
   important because these values are used to compute neighbour masks (see
   may_be_bridge() for details). */
static int dr[6] = {  0, -1, -1,  0, +1, +1 };
static int dc[6] = { +1,  0, -1, -1,  0, +1 };

static int max(int i, int j) { return i > j ? i : j; }

static int distance(int r1, int c1, int r2, int c2)
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

EXTERN void board_place(Board *board, const Place *p)
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

EXTERN void board_unplace(Board *board, const Place *p)
{
	Field *f = &board->fields[p->r][p->c];
	f->player = NONE;
	f->pieces = 0;
	f->dvonns = 0;
	--board->moves;
}

/* Temporary space shared by mark_reachable() and remove_reachable(): */
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
		int nr = r + dr[d], nc = c + dc[d];
		if (nr >= 0 && nr < H && nc >= 0 && nc < W &&
			!board->fields[nr][nc].removed) mask |= 1<<d;
	}
	return bridge_index[mask];
}

EXTERN void board_move(Board *board, const Move *m, Color *old_player)
{
	if (!move_is_pass(m)) {
		Field *f = &board->fields[m->r1][m->c1];
		Field *g = &board->fields[m->r2][m->c2];
		if (old_player) *old_player = g->player;
		g->player = f->player;
		g->pieces += f->pieces;
		g->dvonns += f->dvonns;
		f->removed = board->moves;
		/* We must remove disconnected fields, but since remove_unreachable()
		   is expensive, we try to elide it if permissible: */
		if (f->dvonns || may_be_bridge(board, m->r1, m->c1)) {
			remove_unreachable(board);
		}
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

EXTERN void board_unmove(Board *board, const Move *m, Color old_player)
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

EXTERN void board_validate(const Board *board)
{
	int r, c;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			const Field *f = &board->fields[r][c];
			assert(f->pieces <= N);
			assert(f->dvonns <= D);
			assert(f->dvonns <= f->pieces);
			assert(f->dvonns == f->pieces || f->pieces == 0 ?
				f->player == NONE : f->player == WHITE || f->player == BLACK);
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

EXTERN int generate_places(const Board *board, Place places[N])
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

EXTERN void generate_all_moves( const Board *board,
	Move *moves1, Move *moves2, int *nmove1, int *nmove2 )
{
	const Field *f, *g;
	int r1, c1, d, r2, c2;
	Move *moves[2] = { moves1, moves2 }, **pmoves;

	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			f = &board->fields[r1][c1];
			if (f->removed || f->player == NONE) continue;
			pmoves = &moves[f->player];
			if (!*pmoves) continue;
			if (mobile(board, r1, c1)) {
				for (d = 0; d < 6; ++d) {
					r2 = r1 + f->pieces*dr[d];
					c2 = c1 + f->pieces*dc[d];
					if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W) {
						g = &board->fields[r2][c2];
						if (!g->removed) {
							Move new_move = { r1, c1, r2, c2};
							*(*pmoves)++ = new_move;
						}
					}
				}
			}
		}
	}

	if (moves1 != NULL) {
		if (moves1 == moves[0]) *moves[0]++ = move_pass;
		*nmove1 = moves[0] - moves1;
	}
	if (moves2 != NULL) {
		if (moves2 == moves[1]) *moves[1]++ = move_pass;
		*nmove2 = moves[1] - moves2;
	}
}

EXTERN int generate_moves(const Board *board, Move moves[M])
{
	int nmove;
	if (((board->moves - N)&1) == 0) {
		generate_all_moves(board, moves, NULL, &nmove, NULL);
	} else {
		generate_all_moves(board, NULL, moves, NULL, &nmove);
	}
	return nmove;
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

EXTERN int board_score(const Board *board)
{
	int sc[2], player = next_player(board);
	board_scores(board, sc);
	return sc[player] - sc[1 - player];
}
