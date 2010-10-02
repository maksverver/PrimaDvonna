#include "IO.h"
#include <assert.h>
#include <string.h>

EXTERN bool parse_move(const char *text, Move *move)
{
	if (strcmp(text, "PASS") == 0) {
		*move = move_pass;
		return true;
	}
	if ( text[0] >= 'A' && text[0] <= 'K' &&
	     text[1] >= '1' && text[1] <= '5' )
	{
		if (text[2] == '\0') {
			move->r1 = text[1] - '1';
			move->c1 = text[0] - 'A';
			move->r2 = -1;
			move->c2 = -1;
			return true;
		}
		if ( text[2] >= 'A' && text[2] <= 'K' &&
		     text[3] >= '1' && text[3] <= '5' && text[4] == '\0' ) {
			move->c1 = text[0] - 'A';
			move->r1 = text[1] - '1';
			move->c2 = text[2] - 'A';
			move->r2 = text[3] - '1';
			return true;
		}
	}
	return false;
}

EXTERN const char *format_move(const Move *move)
{
	if (move_passes(move)) {  /* pass */
		return "PASS";
	} else {  /* place or stack */
		static char buf[5];
		char *p = buf;

		*p++ = 'A' + move->c1;
		*p++ = '1' + move->r1;
		if (move_stacks(move)) {
			*p++ = 'A' + move->c2;
			*p++ = '1' + move->r2;
		}
		*p++ = '\0';
		return buf;
	}
}

static const char *digits =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

EXTERN bool parse_state(const char *descr, Board *board, Color *next_player)
{
	int vals[N + 1], n, r, c;

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

	/* The next N values rae used to initialize the fields of the board;
	   board->moves is initialized to something sensible based on the number
	   of occupied (when placing) or empty (when stacking) fields. */
	n = 1;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			Field *f = &board->fields[r][c];

			if (!f->removed) {
				assert(n < N + 1);
				if (vals[n] == 0) {
					if (board->moves >= N) {
						f->removed = N;
						f->pieces = 1;
						f->player = 0;
						++board->moves;
					}
				} else if (vals[n] == 1) {
					f->dvonns = 1;
					f->pieces = 1;
					if (board->moves < N) ++board->moves;
				} else {
					f->player = (vals[n] + 2)%2;
					f->dvonns = (vals[n] + 2)/2%2;
					f->pieces = (vals[n] + 2)/4;
					if (board->moves < N) ++board->moves;
				}
				++n;
			}
		}
	}
	assert(n == N + 1);

	/* Because of the disconnection rule, fewer moves may have been played in
	   the stacking phase than the number of empty fields suggest. If an odd
	   number of stacks have been removed this way, we must adjust the move
	   counter by one to create a consistent game state. */
	if (*next_player != NONE && next_player(board) != *next_player) {
		--board->moves;
		assert(next_player(board) == *next_player);
		assert(board->moves > N);
	}
	return true;
}

EXTERN const char *format_state(const Board *board)
{
	Board init_board;
	static char buf[N + 2];
	int r, c, n;

	board_clear(&init_board);
	if (board->moves < N) {  /* placement phase */
		buf[0] = digits[board->moves%2];
	} else {  /* movement phase */
		buf[0] = digits[2 + (board->moves - N)%2];
	}
	n = 1;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			if (!init_board.fields[r][c].removed) {
				const Field *f = &board->fields[r][c];

				assert(n < N + 1);
				if (f->pieces > 15) {  /* stack too high for this encoding! */
					buf[n] = '*';
				} else if (f->removed || !f->pieces) {
					buf[n] = digits[0];
				} else if (f->player == NONE) {
					buf[n] = digits[1];
				} else {
					buf[n] = digits[4*f->pieces + (f->dvonns?2:0) + f->player - 2];
				}
				++n;
			}
		}
	}
	assert(n == N + 1);
	buf[n] = '\0';
	return buf;
}
