#include "Crash.h"
#include "Game.h"
#include "AI.h"
#include "Time.h"
#include "TT.h"
#include "IO.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define default_depth 12
#define default_time 4.88

static int arg_seed = 0;
static const char *arg_state = NULL;
static int arg_depth = default_depth;
static double arg_time = default_time;

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
		time_stop();
		if (fgets(line, sizeof(line), stdin) == NULL) {
			fprintf(stderr, "Unexpected end of input!\n");
			exit(EXIT_FAILURE);
		}
		time_start();
		if (*trim(line)) {
			if (strcmp(line, "Quit") == 0) {
				/* Non-standard extension: my game server send the game result
				   in human-readable form on a single line after Quit: */
				/*
				if (fgets(line, sizeof(line), stdin) != NULL) {
					fputs(line, stderr);
				}
				*/
				fprintf(stderr, "%.3fs\nQuit received! Exiting.\n", time_used());
				fflush(stderr);
				exit(EXIT_SUCCESS);
			} else {
				return line;
			}
		}
	}
}

/* Parses a move string and then executes it.
   To verify internal consistency, we check that the move is indeed valid
   according to the current board state, even though the arbiter already
   guarantees that no invalid moves will be passed to the players. */
static void parse_and_execute_move(Board *board, const char *line)
{
	Move move;

	if (!parse_move(line, &move)) {
		fprintf(stderr, "Could not parse move: %s!\n", line);
		exit(EXIT_FAILURE);
	} else if (!valid_move(board, &move)) {
		fprintf(stderr, "Invalid move: %s!\n", line);
		exit(EXIT_FAILURE);
	} else {
		board_do(board, &move);
	}
	board_validate(board);
}

static void run_game()
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
		if (next_player(&board) == my_color) {  /* it's my turn */
			Move move;
			fprintf(stderr, "%s\n", format_state(&board));
			if (!ai_select_move(&board, &move)) {
				fprintf(stderr, "Internal error: no move selected!\n");
				exit(EXIT_FAILURE);
			}
			move_str = format_move(&move);
		}
		if (move_str) {  /* send my move */
			fprintf(stderr, " --%s-->\n", move_str);
			printf("%s\n", move_str);
		} else {  /* receive opponent's move */
			move_str = read_line();
			fprintf(stderr, "<--%s--\n", move_str);
		}
		parse_and_execute_move(&board, move_str);
	}
}

static void solve_state(const char *descr)
{
	Color next_player;
	Board board;
	Move move, pv[40];
	const char *move_str = NULL;
	int depth = 0, n, npv;

	if (!parse_state(descr, &board, &next_player)) {
		fprintf(stderr, "Couldn't parse game description: `%s'!\n", descr);
		exit(EXIT_FAILURE);
	}
	board_validate(&board);
	fprintf(stderr, "Intermediate value: %d\n", ai_evaluate(&board));
	if (next_player == NONE) {
		fprintf(stderr, "Game already finished!\n");
	} else {
		board_validate(&board);
		for (depth = arg_depth<2 ? arg_depth : 2; depth <= arg_depth; ++depth) {
			if (!ai_select_move_fixed(&board, &move, depth)) {
				fprintf(stderr, "Internal error: no move selected!\n");
				exit(EXIT_FAILURE);
			}
			fprintf(stderr, "%d:", depth);
			npv = ai_extract_pv(&board, pv, sizeof(pv)/sizeof(*pv));
			assert(npv > 0 && move_compare(&move, &pv[0]) == 0);
			for (n = 0; n < npv; ++n) printf(" %s", format_move(&pv[n]));
			printf("\n");
		}
		move_str = format_move(&move);
	}
	if (move_str) {
		printf("%s\n", move_str);
		parse_and_execute_move(&board, move_str);
		fprintf(stderr, "New state: %s\n", format_state(&board));
	}
}

static void print_usage()
{
	printf(
		"Usage:\n"
		"\tplayer <options>\n"
		"Options:\n"
		"\t--help            display this help message and exit\n"
		"\t--seed=<int>      "
	"initialize RNG with given seed (default: random)\n"
		"\t--state=<descr>   solve given state\n"
		"\t--depth=<depth>   maximum search depth (default: %d)\n"
		"\t--time=<time>     maximum time per game (default: %.2fs)\n"
		"\t--enable-tt       enable transposition table\n"
		"\t--disable-tt      disable transposition table\n"
		"\t--enable-mo       enable move ordering\n"
		"\t--disable-mo      disable move ordering\n",
		default_depth, default_time );
}

static void parse_args(int argc, char *argv[])
{
	int pos;

	for (pos = 1; pos < argc && argv[pos][0] == '-'; ++pos)
	{
		if (sscanf(argv[pos], "--seed=%d", &arg_seed) == 1) {
			continue;
		}
		if (strcmp(argv[pos], "--help") == 0) {
			pos += 1;
			print_usage();
			exit(EXIT_SUCCESS);
		}
		if (memcmp(argv[pos], "--state=", 8) == 0) {
			arg_state = argv[pos] + 8;
			continue;
		}
		if (sscanf(argv[pos], "--depth=%d", &arg_depth) == 1) {
			continue;
		}
		if (sscanf(argv[pos], "--time=%lf", &arg_time) == 1) {
			continue;
		}
		if (strcmp(argv[pos], "--enable-tt") == 0) {
			ai_use_tt = true;
			continue;
		}
		if (strcmp(argv[pos], "--disable-tt") == 0) {
			ai_use_tt = false;
			continue;
		}
		if (strcmp(argv[pos], "--enable-mo") == 0) {
			ai_use_mo = true;
			continue;
		}
		if (strcmp(argv[pos], "--disable-mo") == 0) {
			ai_use_mo = false;
			continue;
		}
		if (strcmp(argv[pos], "--enable-killer") == 0) {
			ai_use_killer = true;
			continue;
		}
		if (strcmp(argv[pos], "--disable-killer") == 0) {
			ai_use_killer = false;
			continue;
		}
		break;
	}
	if (pos < argc) {
		printf("Invalid command line argument: `%s'!\n\n", argv[pos]);
		print_usage();
		exit(EXIT_FAILURE);
	}
}

int main(int argc, char *argv[])
{
	/* Initialize timer, as early as possible! */
	time_restart();

	/* Set-up crash handler. */
	crash_init();

	/* Parse command line arguments. */
	parse_args(argc, argv);

	/* Set time limit: */
	time_limit = arg_time;

	/* Make stdout and stderr line buffered: */
	{
		static char buf_stdout[512], buf_stderr[512];
		setvbuf(stdout, buf_stdout, _IOLBF, sizeof(buf_stdout));
		setvbuf(stderr, buf_stderr, _IOLBF, sizeof(buf_stderr));
	}

	/* Initialize RNG: */
	if (!arg_seed) arg_seed = (1337*(int)getpid() + 17*(int)time(NULL))%1000000;
	srand(arg_seed);
	fprintf(stderr, "RNG seed %d.\n", arg_seed);

	/* Initialize transposition table: */
	if (ai_use_tt) {
		tt_init(3000007);
		fprintf(stderr, "%.3f MB transposition table is enabled.\n",
			1.0*tt_size*sizeof(TTEntry)/1024/1024);
	} else {
		fprintf(stderr, "Transposition table is disabled.\n");
	}

	/* Print other parameters: */
	fprintf(stderr, "Move ordering is %sabled.\n",
		ai_use_mo ? "en" : "dis");
	fprintf(stderr, "Killer heuristic is %sabled.\n",
		ai_use_mo ? "en" : "dis");

	fprintf(stderr, "Initialization took %.3fs.\n", time_used());

	if (arg_state) {
		solve_state(arg_state);
	} else {
		run_game();
	}

	if (ai_use_tt) tt_fini();
	crash_fini();

	return EXIT_SUCCESS;
}
