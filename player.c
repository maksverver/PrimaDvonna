#include "AI.h"
#include "Game.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static char *trim(char *s)
{
	char *p = s, *q = s + strlen(s);
	while (q > p && isspace(q[-1])) --q;
	*q = '\0';
	while (*p && isspace(*p)) ++p;
	if (p > s) memmove(s, p, q - p + 1);
	return s;
}

static const char *read_line(void)
{
	static char line[1024];
	for (;;) {
		if (fgets(line, sizeof(line), stdin) == NULL) {
			fprintf(stderr, "Unexpected end of input!\n");
			exit(EXIT_FAILURE);
		}
		if (*trim(line)) {
			if (strcmp(line, "Quit") == 0) {
				/* Non-standard extension: my game server send the game result
				   in human-readable form on a single line after Quit: */
				if (fgets(line, sizeof(line), stdin) != NULL) {
					fputs(line, stderr);
				}
				exit(EXIT_SUCCESS);
			} else {
				return line;
			}
		}
	}
}

static bool parse_place(const char *line, Place *place)
{
	if (line[0] >= 'A' && line[0] <= 'K' &&
		line[1] >= '1' && line[1] <= '5' && line[2] == '\0') {
		place->r = line[1] - '1';
		place->c = line[0] - 'A';
		return true;
	}
	return false;
}

static bool parse_move(const char *line, Move *move)
{
	if (strcmp(line, "PASS") == 0) {
		*move = move_pass;
		return true;
	}
	if (line[0] >= 'A' && line[0] <= 'K' && line[1] >= '1' && line[1] <= '5' &&
		line[2] >= 'A' && line[2] <= 'K' && line[3] >= '1' && line[3] <= '5' &&
		line[4] == '\0') {
		move->c1 = line[0] - 'A';
		move->r1 = line[1] - '1';
		move->c2 = line[2] - 'A';
		move->r2 = line[3] - '1';
		return true;
	}
	return false;
}

static const char *format_place(const Place *place)
{
	static char buf[3];
	buf[0] = (char)('A' + place->c);
	buf[1] = (char)('1' + place->r);
	buf[2] = '\0';
	return buf;
}

static const char *format_move(const Move *move)
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

/* Parses a move string and then executes it.
   To verify internal consistency, we check that the move is indeed valid
   according to the current board state, even though the arbiter already
   guarantees that no invalid moves will be passed to the players. */
static void parse_and_execute_move(Board *board, const char *line)
{
	if (board->moves < N) {
		Place place, places[N];
		int i, nplace = generate_places(board, places);

		if (!parse_place(line, &place)) {
			fprintf(stderr, "Could not parse placement: %s!\n", line);
			exit(EXIT_FAILURE);
		} else {
			for (i = 0; i < nplace; ++i) {
				if (place.r == places[i].r && place.c == places[i].c) break;
			}
			if (i == nplace) {
				fprintf(stderr, "Invalid placement: %s!\n", line);
				exit(EXIT_FAILURE);
			} else {
				board_place(board, &place);
			}
		}
	} else {
		Move move, moves[M];
		int i, nmove = generate_moves(board, moves);

		if (!parse_move(line, &move)) {
			fprintf(stderr, "Could not parse move: %s!\n", line);
			exit(EXIT_FAILURE);
		} else {
			for (i = 0; i < nmove; ++i) {
				if (move.r1 == moves[i].r1 && move.c1 == moves[i].c1 &&
					move.r2 == moves[i].r2 && move.c2 == moves[i].c2) break;
			}
			if (i == nmove) {
				fprintf(stderr, "Invalid move: %s!\n", line);
				exit(EXIT_FAILURE);
			} else {
				board_move(board, &move, NULL);
			}
		}
	}
	board_validate(board);
}

static void run()
{
	Color my_color;
	Board board;
	const char *move_str;

	board_clear(&board);
	board_validate(&board);
	move_str = read_line();
	if (strcmp(move_str, "Start") == 0) {
		my_color = WHITE;
	} else {
		parse_and_execute_move(&board, move_str);
		my_color = BLACK;
	}
	for (;;) {
		move_str = NULL;
		if (board.moves < N) {
			if ((board.moves & 1) == my_color) {
				Place place;
				if (!ai_select_place(&board, &place)) {
					fprintf(stderr, "Internal error: no placement selected!\n");
					exit(EXIT_FAILURE);
				}
				move_str = format_place(&place);
			}
		} else {
			if (((board.moves - N) & 1) == my_color) {
				Move move;
				if (!ai_select_move(&board, &move)) {
					fprintf(stderr, "Internal error: no move selected!\n");
					exit(EXIT_FAILURE);
				}
				move_str = format_move(&move);
			}
		}
		if (move_str) {
			printf("%s\n", move_str);
		} else {
			move_str = read_line();
		}
		parse_and_execute_move(&board, move_str);
	}
}

int main()
{
	static char buf_stdout[512], buf_stderr[512];

	/* Make stdout and stderr line buffered: */
	setvbuf(stdout, buf_stdout, _IOLBF, sizeof(buf_stdout));
	setvbuf(stderr, buf_stderr, _IOLBF, sizeof(buf_stderr));

	/* Initialize RNG: */
	srand((int)time(NULL) + 1337*(int)getpid());

	run();

	return EXIT_SUCCESS;
}
