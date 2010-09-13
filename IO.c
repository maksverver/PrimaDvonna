#include "IO.h"
#include <assert.h>
#include <string.h>

bool parse_place(const char *text, Place *place)
{
	if (text[0] >= 'A' && text[0] <= 'K' &&
		text[1] >= '1' && text[1] <= '5' && text[2] == '\0') {
		place->r = text[1] - '1';
		place->c = text[0] - 'A';
		return true;
	}
	return false;
}

bool parse_move(const char *text, Move *move)
{
	if (strcmp(text, "PASS") == 0) {
		*move = move_pass;
		return true;
	}
	if (text[0] >= 'A' && text[0] <= 'K' && text[1] >= '1' && text[1] <= '5' &&
		text[2] >= 'A' && text[2] <= 'K' && text[3] >= '1' && text[3] <= '5' &&
		text[4] == '\0') {
		move->c1 = text[0] - 'A';
		move->r1 = text[1] - '1';
		move->c2 = text[2] - 'A';
		move->r2 = text[3] - '1';
		return true;
	}
	return false;
}

const char *format_place(const Place *place)
{
	static char buf[3];
	buf[0] = (char)('A' + place->c);
	buf[1] = (char)('1' + place->r);
	buf[2] = '\0';
	return buf;
}

const char *format_move(const Move *move)
{
	static char buf[5];
	if (move_is_pass(move)) {
		strcpy(buf, "PASS");
	} else {
		buf[0] = 'A' + move->c1;
		buf[1] = '1' + move->r1;
		buf[2] = 'A' + move->c2;
		buf[3] = '1' + move->r2;
		buf[4] = '\0';
	}
	return buf;
}

static const char *digits =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

bool parse_state(const char *descr, Board *board, Color *next_player)
{
	int vals[N + 1], n, r, c;

	if (strlen(descr) != N + 1) return false;
	for (n = 0; n < N + 1; ++n) {
		const char *p = strchr(digits, descr[n]);
		if (p == NULL) return false;
		vals[n] = (int)(p - digits);
	}

	board_clear(board);
	*next_player = vals[0]%2;
	switch (vals[0]/2%3)
	{
	case 0: board->moves = *next_player; break;
	case 1: board->moves = N + *next_player; break;
	default: *next_player = NONE; break;
	}
	n = 1;
	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			Field *f = &board->fields[r][c];

			if (!f->removed) {
				assert(n < N + 1);
				if (vals[n] > 0) {
					if (vals[n] == 1) {
						f->dvonns = 1;
						f->pieces = 1;
					} else {
						f->player = (vals[n] + 2)%2;
						f->dvonns = (vals[n] + 2)/2%2;
						f->pieces = (vals[n] + 2)/4;
					}
				}
				++n;
			}
		}
	}
	assert(n == N + 1);
	return true;
}
