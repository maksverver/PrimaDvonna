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

#define default_player_time 4.95

static int         arg_seed      = 0;
static const char *arg_state     = NULL;
static int         arg_color     = -1;
static int         arg_analyze   = 0;
static AI_Limit    arg_limit     = { 0, 0, 0.0 };

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

#if 0
static int est_moves_left(const Board *board, Color player)
{
	int r1, c1, r2, c2, d, res = 0;
	const Field *f;

	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			f = &board->fields[r1][c1];
			if (!f->removed && f->player == player) {
				for (d = 0; d < 6; ++d) {
					r2 = r1 + DR[d]*f->pieces;
					if (r2 >= 0 && r2 < H) {
						c2 = c1 + DC[d]*f->pieces;
						if (c2 >= 0 && c2 < W) {
							if (!board->fields[r2][c2].removed) {
								++res;
								break;
							}
						}
					}
				}
			}
		}
	}
	return res;
}
#endif

static int max_moves_left(const Board *board)
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

static bool select_move(Board *board, Move *move)
{
	bool ok;
	AI_Result result;
	AI_Limit limit = arg_limit;

	if (board->moves < N) {
		/* Placement phase. */
		limit.eval  = 1000;
		limit.depth = N - board->moves;
		limit.time  = 0;
		ok = ai_select_move(board, &limit, &result);
	} else {
		/* Stacking phase: divide remaining time over est. moves to play: */
		if (!limit.time && !limit.depth && !limit.eval)
		{
#if 0
			/* "New" and "improved" budget code that actually performs worse
			   than the old version, below! I should try to figure out why,
			   before changing the budget code again, and even then test very
			   carefully to ensure the change is actually an improvement. */
			int d = est_moves_left(board, next_player(board)) - 6;
			if (d > 12) d = 12;
			if (d <  3) d =  3;
#endif
			int d = (max_moves_left(board) - 16)/2;
			if (d < 3) d = 3;
			limit.time = time_left()/d;
			fprintf(stderr, "%.3fs+%.3fs\n", time_used(), limit.time);
		}
		ok = ai_select_move(board, &limit, &result);
	}
	if (ok) *move = result.move;
	return ok;
}

static void run_game(Board *board, int my_colors)
{
	const char *move_str;

	/* In player mode, use the time limit as the global time limit: */
	time_limit = (arg_limit.time > 0) ? arg_limit.time : default_player_time;
	arg_limit.time = 0;
	while (generate_all_moves(board, NULL) > 0) {
		move_str = NULL;
		if (my_colors & (1 << (int)next_player(board))) {  /* it's my turn */
			Move move;
			fprintf(stderr, "%s\n", format_state(board));
			if (!select_move(board, &move)) {
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
		parse_and_execute_move(board, move_str);
	}
}

static void analyze(Board *board, Color next_player)
{
	AI_Result result;
	Move pv[AI_MAX_DEPTH];
	int n, npv;

	fprintf(stderr, "Intermediate value: "VAL_FMT"\n", ai_evaluate(board));
	if (next_player == NONE) {
		fprintf(stderr, "Game already finished!\n");
	} else {
		if (!ai_select_move(board, &arg_limit, &result)) {
			fprintf(stderr, "Internal error: no move selected!\n");
			exit(EXIT_FAILURE);
		}
		board_validate(board);
		npv = ai_extract_pv(board, pv, AI_MAX_DEPTH);
		fprintf(stderr, "Principal variation:");
		for (n = 0; n < npv; ++n) {
			fprintf(stderr, " %s", format_move(&pv[n]));
		}
		fprintf(stderr, "\n");
		board_do(board, &result.move);
		fprintf(stderr, "New state: %s\n", format_state(board));
		board_undo(board, &result.move);
		printf("%s\n", format_move(&result.move));
	}
}

static void print_usage(void)
{
	printf(
		"Usage:\n"
		"\tplayer <options>\n"
		"Options:\n"
		"\t--help            display this help message and exit\n"
		"\t--seed=<int>      "
	"initialize RNG with given seed (default: random)\n"
		"\t--state=<descr>   initialize board to given state\n"
		"\t--color=<num>     color to play "
			"(0: none, 1: white, 2: black, 3: both)\n"
		"\t--analyze         analyze this position only\n"
		"\t--depth=<depth>   stop after searching on given depth \n"
		"\t--eval=<count>    "
	"stop after evaluating given number of positions\n"
		"\t--time=<time>     "
	"maximum time per game/state (default when playing: %.2fs)\n"
		"\t--tt=<size>       set use of table (0: off)\n"
		"\t--mo=<val>        enable move ordering "
			"(0: off, 1: heuristic, 2: evaluated)\n"
		"\t--killer=<val>    enable killer heuristic "
			"(0: off, 1: one ply, 2: two ply)\n"
		"\t--pvs=<val>       enable principal variation search"
			"(0: off, 1: on)\n"
		"\t--weights=a:..:e  set evaluation function weights\n",
		default_player_time );
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
		if (sscanf(argv[pos], "--color=%d", &arg_color) == 1) continue;
		if (strcmp(argv[pos], "--analyze") == 0) {
			arg_analyze = 1;
			continue;
		}
		if (sscanf(argv[pos], "--depth=%d", &arg_limit.depth) == 1) continue;
		if (sscanf(argv[pos], "--eval=%d", &arg_limit.eval) == 1) continue;
		if (sscanf(argv[pos], "--time=%lf", &arg_limit.time) == 1) continue;
		if (sscanf(argv[pos], "--tt=%d", &ai_use_tt) == 1) continue;
		if (sscanf(argv[pos], "--mo=%d", &ai_use_mo) == 1) continue;
		if (sscanf(argv[pos], "--killer=%d", &ai_use_killer) == 1) continue;
		if (sscanf(argv[pos], "--pvs=%d", &ai_use_pvs) == 1) continue;
		if (sscanf(argv[pos], "--weights=" VAL_FMT":"VAL_FMT":"VAL_FMT":"VAL_FMT":"VAL_FMT,
			&eval_weights.stacks, &eval_weights.score, &eval_weights.moves,
			&eval_weights.to_life, &eval_weights.to_enemy) == 5) {
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

static void print_memory_use(void)
{
	char line[1024];
	FILE *fp;
	long a, b, bytes = 0;

	fp = fopen("/proc/self/maps", "rt");
	if (fp == NULL) {
		fprintf(stderr, "Couldn't open /proc/self/maps!\n");
	} else {
		while (fgets(line, sizeof(line), fp) != NULL) {
			if (sscanf(line, "%lx-%lx ", &a, &b) == 2) {
				bytes += b - a;
			} else {
				fprintf(stderr, "Couldn't parse line: %s!\n", line);
				break;
			}
		}
		fclose(fp);
	}
	fprintf(stderr, "Memory used: %ld bytes (%.3f MB)\n",
		bytes, 1.0*bytes/(1<<20));
}

int main(int argc, char *argv[])
{
	Board board;
	Color next_player;

	/* Initialize timer, as early as possible! */
	time_restart();

	/* Set-up crash handler. */
	crash_init();

	/* Parse command line arguments. */
	parse_args(argc, argv);

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
	if (ai_use_tt > 0) {
		tt_init(1<<ai_use_tt);  /* 2 M entries = 48 MB at 24 bytes per entry */
		fprintf(stderr, "%.3f MB transposition table is enabled.\n",
			1.0*tt_size*sizeof(TTEntry)/1024/1024);
	} else {
		fprintf(stderr, "Transposition table is disabled.\n");
		ai_use_killer = 0;  /* implicit */
	}

	/* Print other parameters: */
	fprintf(stderr, "Move ordering is %s.\n",
		ai_use_mo == 0 ? "disabled" :
		ai_use_mo == 1 ? "heuristic" :
		ai_use_mo == 2 ? "evaluated" : "invalid");
	fprintf(stderr, "Killer heuristic is %s.\n",
		ai_use_killer == 0 ? "disabled" :
		ai_use_killer == 1 ? "one ply" :
		ai_use_killer == 2 ? "two ply" : "invalid");
	fprintf(stderr, "Principal variation search is %s.\n",
		ai_use_pvs == 0 ? "disabled" :
		ai_use_pvs == 1 ? "enabled" : "invalid" );
	print_memory_use();

	fprintf(stderr, "Initialization took %.3fs.\n", time_used());

	/* Set-up initial game state */
	board_clear(&board);
	next_player = WHITE;
	if (arg_state) {
		if (!parse_state(arg_state, &board, &next_player)) {
			fprintf(stderr, "Couldn't parse state: `%s'!\n", arg_state);
			exit(EXIT_FAILURE);
		}
	}
	board_validate(&board);
	
	/* Run main program: */
	if (arg_analyze) {
		analyze(&board, next_player);
	} else {
		if (arg_color < 0) {
			const char *move_str = read_line();
			if (strcmp(move_str, "Start") == 0) {
				arg_color = 1 << next_player;
			} else {
				parse_and_execute_move(&board, move_str);
				arg_color = 1 << (1 - next_player);
			}
		}
		run_game(&board, arg_color);
	}

	/* Clean up: */
	if (ai_use_tt) tt_fini();
	crash_fini();

	return EXIT_SUCCESS;
}
