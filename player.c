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

#define default_player_time 4.88

static int         arg_seed      = 0;
static const char *arg_state     = NULL;
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

/* For debugging: quick hack to get a setup move from Dvonner: */
static void get_dvonner_setup_move(const Board *board, Move *move)
{
	int r, c;
	const Field *f;
	char moves[3][N][3];
	int nmove[3] = { 0, 0, 0 }, n;
	char command[500], line[500];

	for (r = 0; r < H; ++r) {
		for (c = 0; c < W; ++c) {
			f = &board->fields[r][c];
			if (!f->removed && f->pieces) {
				int p = f->player + 1, i = nmove[p]++;
				moves[p][i][0] = 'A' + c;
				moves[p][i][1] = '1' + r;
				moves[p][i][2] = '\0';
			}
		}
	}
	assert(nmove[1] == nmove[2] || nmove[1] + 1 == nmove[2]);
	assert(nmove[0] <= D);
	assert(nmove[0] == D || (nmove[1] == 0 && nmove[2] == 0));

	/* Write temporary Dvonner savegame file: */
	FILE *fp = fopen("tmp.dvg", "wt");
	assert(fp != NULL);
	for (n = 0; n < nmove[0] + nmove[1] + nmove[2]; ++n) {
		fprintf(fp, "%s%s",
			(n < D) ? moves[0][n] : moves[1 + n%2][(n - D)/2],
			(n%2 == 0) ? "  " : "\n" );
	}
	fprintf(fp, "BREAK\n");
	fclose(fp);

	/* Execute Dvonner to obtain move: */
	sprintf(command,
		"printf '/stop\n/load tmp\n/color %s\n/start\n/stop\n/quit\n'"
		" | players/dvonner.bin "
		" | grep 'Dvonner does setup move' | grep -o '[A-K][1-5]'",
		(next_player(board) == WHITE) ? "black" : "white");
	fp = popen(command, "r");
	if (fp == NULL) {
		fprintf(stderr, "Failed to execute Dvonner!\n");
		exit(EXIT_FAILURE);
	}
	if (fgets(line, sizeof(line), fp) == NULL) {
		fclose(fp);
		fprintf(stderr, "Failed to read from Dvonner!\n");
		exit(EXIT_FAILURE);
	}
	fclose(fp);
	line[2] = '\0';
	if (!parse_move(line, move)) {
		fprintf(stderr, "Could not parse move!\n");
		exit(EXIT_FAILURE);
	}
}

static bool select_move(Board *board, Move *move)
{
	bool ok;
	AI_Result result;
	AI_Limit limit = arg_limit;

	if (board->moves < N) {
#if 1
		/* Placement phase: just greedily pick best move. */
		limit.depth = 1;
		ok = ai_select_move(board, &limit, &result);
#else /* DEBUG */
		get_dvonner_setup_move(board, move);
		return true;
#endif
	} else {
#if 1
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
#else  /* Dvonner mode: */
		assert(limit.eval > 0 && !limit.time && !limit.depth);
		for (;;) {
			limit.depth = depth;
			ok = ai_select_move(board, &limit, &result);
			if (!ok || result.eval >= limit.eval || depth == 20) break;
			++depth;
		}
#endif
	}
	if (ok) *move = result.move;
	return ok;
}

static void run_game()
{
	Color my_color;
	Board board;
	const char *move_str;

	/* In player mode, use the time limit as the global time limit: */
	time_limit = (arg_limit.time > 0) ? arg_limit.time : default_player_time;
	arg_limit.time = 0;
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
			if (!select_move(&board, &move)) {
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
	AI_Result result;
	Board board;
	Move pv[40];
	int n, npv;

	if (!parse_state(descr, &board, &next_player)) {
		fprintf(stderr, "Couldn't parse game description: `%s'!\n", descr);
		exit(EXIT_FAILURE);
	}
	board_validate(&board);
	fprintf(stderr, "Intermediate value: "VAL_FMT"\n", ai_evaluate(&board));
	if (next_player == NONE) {
		fprintf(stderr, "Game already finished!\n");
	} else {
		board_validate(&board);
		if (!ai_select_move(&board, &arg_limit, &result)) {
			fprintf(stderr, "Internal error: no move selected!\n");
			exit(EXIT_FAILURE);
		}
		npv = ai_extract_pv(&board, pv, sizeof(pv)/sizeof(*pv));
		fprintf(stderr, "Principal variation:");
		for (n = 0; n < npv; ++n) {
			fprintf(stderr, " %s", format_move(&pv[n]));
		}
		fprintf(stderr, "\n");
		board_do(&board, &result.move);
		fprintf(stderr, "New state: %s\n", format_state(&board));
		printf("%s\n", format_move(&result.move));
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
		"\t--depth=<depth>   stop after searching on given depth \n"
		"\t--eval=<count>    "
	"stop after evaluating given number of positions\n"
		"\t--time=<time>     "
	"maximum time per game/state (default when playing: %.2fs)\n"
		"\t--enable-tt       enable transposition table\n"
		"\t--disable-tt      disable transposition table\n"
		"\t--enable-mo       enable move ordering\n"
		"\t--disable-mo      disable move ordering\n",
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
		if (sscanf(argv[pos], "--depth=%d", &arg_limit.depth) == 1) {
			continue;
		}
		if (sscanf(argv[pos], "--eval=%d", &arg_limit.eval) == 1) {
			continue;
		}
		if (sscanf(argv[pos], "--time=%lf", &arg_limit.time) == 1) {
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
		tt_init(3000017);
		fprintf(stderr, "%.3f MB transposition table is enabled.\n",
			1.0*tt_size*sizeof(TTEntry)/1024/1024);
	} else {
		fprintf(stderr, "Transposition table is disabled.\n");
		ai_use_killer = false;
	}

	/* Print other parameters: */
	fprintf(stderr, "Move ordering is %sabled.\n",
		ai_use_mo ? "en" : "dis");
	fprintf(stderr, "Killer heuristic is %sabled.\n",
		ai_use_killer ? "en" : "dis");
	fprintf(stderr, "Initialization took %.3fs.\n", time_used());

	/* Run main program: */
	if (arg_state) solve_state(arg_state);
	else run_game();

	/* Clean up: */
	if (ai_use_tt) tt_fini();
	crash_fini();

	return EXIT_SUCCESS;
}
