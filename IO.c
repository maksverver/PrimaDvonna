#include "IO.h"
#include <assert.h>
#include <string.h>

static const int field_row[49] = {
	     0,  0,  0,  0,  0,  0,  0,  0,  0,
	   1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
	 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,
	   3,  3,  3,  3,  3,  3,  3,  3,  3,  3,
	     4,  4,  4,  4,  4,  4,  4,  4,  4 };

static const int field_col[49] = {
	     0,  1,  2,  3,  4,  5,  6,  7,  8,
	   0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
	   1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
	     2,  3,  4,  5,  6,  7,  8,  9, 10 };

static const int field_index[5][11] = {
	{  0,  1,  2,  3,  4,  5,  6,  7,  8, -1, -1 },
	{  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, -1 },
	{ 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29 },
	{ -1, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39 },
	{ -1, -1, 40, 41, 42, 43, 44, 45, 46, 47, 48 } };

bool parse_move(const char *text, Move *move)
{
	if (strcmp(text, "PASS") == 0) {
		*move = move_pass;
		return true;
	}
	if ( text[0] >= 'A' && text[0] <= 'K' &&
	     text[1] >= '1' && text[1] <= '5' )
	{
		if (text[2] == '\0') {
			move->src = field_index[text[1] - '1'][text[0] - 'A'];
			move->dst = -1;
			return true;
		}
		if ( text[2] >= 'A' && text[2] <= 'K' &&
		     text[3] >= '1' && text[3] <= '5' && text[4] == '\0' ) {
			move->src = field_index[text[1] - '1'][text[0] - 'A'];
			move->dst = field_index[text[3] - '1'][text[2] - 'A'];
			return true;
		}
	}
	return false;
}

const char *format_move(const Move *move)
{
	if (move_passes(move)) {  /* pass */
		return "PASS";
	} else {  /* place or stack */
		static char buf[5];
		char *p = buf;

		*p++ = 'A' + field_col[move->src];
		*p++ = '1' + field_row[move->src];
		if (move_stacks(move)) {
			*p++ = 'A' + field_col[move->dst];
			*p++ = '1' + field_row[move->dst];
		}
		*p++ = '\0';
		return buf;
	}
}

static const char *digits =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

extern void update_neighbour_mobility(Board *board, int n, int diff);

bool parse_state(const char *descr, Board *board, Color *next_player)
{
	int vals[N + 1], n;

	/* Decode base-62 encoded values: */
	if (strlen(descr) != N + 1) return false;
	for (n = 0; n < N + 1; ++n) {
		const char *p = strchr(digits, descr[n]);
		if (p == NULL) return false;
		vals[n] = (int)(p - digits);
	}

	/* Start with an empty board. */
	board_clear(board);

	/* First value determines the game phase and next player. */
	*next_player = vals[0]%2;
	switch (vals[0]/2%3)
	{
	case 0:  /* placement phase */
		break;
	default:  /* game finished */
		*next_player = NONE;
		/* falls through */
	case 1:  /* movement phase */
		board->moves = N;
		break;
	}

	/* The next N values are used to initialize the fields of the board;
	   board->moves is initialized to something sensible based on the number
	   of occupied (when placing) or empty (when stacking) fields. */
	for (n = 0; n < N; ++n) {
		Field *f = &board->fields[n];
		if (vals[n + 1] == 0) {
			if (board->moves >= N) {
				f->removed = N;
				f->pieces = 1;
				f->player = 0;
				++board->moves;
			}
		} else if (vals[n + 1] == 1) {
			f->dvonns = 1;
			f->pieces = 1;
			if (board->moves < N) ++board->moves;
		} else {
			f->player = (vals[n + 1] + 2)%2;
			f->dvonns = (vals[n + 1] + 2)/2%2;
			f->pieces = (vals[n + 1] + 2)/4;
			if (board->moves < N) ++board->moves;
		}
		if (f->dvonns) board->dvonns |= (1<<n);
		if (vals[n + 1] != 0) update_neighbour_mobility(board, n, -1);
	}

	/* Because of the disconnection rule, fewer moves may have been played in
	   the stacking phase than the number of empty fields suggest. If an odd
	   number of stacks have been removed this way, we must adjust the move
	   counter by one to create a consistent game state. */
	if (*next_player != NONE && next_player(board) != *next_player) {
		--board->moves;
		assert(next_player(board) == *next_player);
		assert(board->moves > N);
	}

#ifdef ZOBRIST
	board->hash = zobrist_hash(board);
#endif

	return true;
}

const char *format_state(const Board *board)
{
	static char buf[N + 2];
	int n;

	if (board->moves < N) {  /* placement phase */
		buf[0] = digits[board->moves%2];
	} else {  /* movement phase */
		buf[0] = digits[2 + (board->moves - N)%2];
	}
	for (n = 0; n < N; ++n) {
		const Field *f = &board->fields[n];

		if (f->pieces > 15) {  /* stack too high for this encoding! */
			buf[n + 1] = '*';
		} else if (f->removed || !f->pieces) {
			buf[n + 1] = digits[0];
		} else if (f->player == NONE) {
			buf[n + 1] = digits[1];
		} else {
			buf[n + 1] = digits[4*f->pieces + (f->dvonns?2:0) + f->player - 2];
		}
	}
	buf[N + 1] = '\0';
	return buf;
}
