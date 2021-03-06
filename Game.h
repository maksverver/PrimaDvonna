#ifndef GAME_H_INCLUDED
#define GAME_H_INCLUDED

#include <stdbool.h>
#include <stdlib.h>

#define N    49             /* maximum number of accessible fields */
#define D     3             /* number of Dvonn pieces */
#define M ((N-D)/2*6)       /* maximum number of moves (for one player) */

typedef enum Color { WHITE = 0, BLACK = 1, NONE = -1 } Color;

/* Type used for hash codes: 64-bit unsigned integers. */
typedef unsigned long long hash_t;

/* A description of the contents of a game field.
   See board_validate() for the invariants that apply to this struct's data. */
typedef struct Field
{
	signed char   player;       /* controlling player's color */
	unsigned char pieces;       /* number of pieces on this field */
	unsigned char dvonns;       /* number of dvonn pieces on this field */
	signed char   removed;      /* move on which field was removed */
	signed char   mobile;       /* number of open neighbouring directions */
} Field;

/* A description of the complete game state. */
typedef struct Board
{
	int            moves;       /* number of moves played so far */
	Field          fields[N];   /* fields of the board */
#ifdef ZOBRIST
	hash_t         hash;        /* hash code for the board */
#endif
	long long      dvonns;      /* bitmask of positions of Dvonns */
} Board;

/* Index of possible moves which can be made with stacks of different heights
   at different positions. board_steps[height][field] points to a zero-
   terminated array of integers, where each integer is an offset relative to
   the current field. e.g. If *board_steps[3][11] == 7, then there is a
   step of size 3 to field 18. */
extern const int *board_steps[50][N];

/* Minimum distance (using single-step moves) between pairs of fields. */
extern const char board_distance[N][N];
#define distance(f, g) (board_distance[(f)][(g)])

/* For each field, gives a bitmask of the six directions (in counter-clockwise
   order) indicating whether there is an adjacent field in that direction; if
   it is less than (1<<6)-1 = 63 then the field is on the edge of the board. */
extern const char board_neighbours[N];

/* This macro determines which color plays next, which is derived from the
    number of moves played so far. (Note that at the end of the placement phase
    white plays twice in succession!) */
#define next_player(b) ((b)->moves < N ? ((b)->moves&1) : ((b)->moves - N)&1)

/* The structure below describes a single move.
   There are three types of moves: placing, stacking and passing.

   `src' is the source field when stacking, or the destination field when
   placing, or -1 when passing. Similarly, `dst' is the destination field
   when stacking, or -1 when placing or passing. */
typedef struct Move {
	signed short src, dst;
} Move;

/* Union of a move and an integer. Used for quick comparisons of moves. */
typedef union MoveInt {
	struct Move m;
	int         i;
} MoveInt;

extern Move move_null;  /* null move: all fields set to 0 */
extern Move move_pass;  /* pass move: all fields set to -1 */
#define move_places(m) ((m)->src >= 0 && (m)->dst < 0)
#define move_stacks(m) ((m)->dst >= 0)
#define move_passes(m) ((m)->src < 0)
#define move_is_null(m) (((union MoveInt)*(m)).i == 0)
#define move_compare(a, b) (((union MoveInt)*(a)).i - ((union MoveInt)*(b)).i)

/* (Re)initialize a board structure to an empty board: */
void board_clear(Board *board);

#ifdef ZOBRIST
/* Recalculates the Zobrist hash of a board from scratch and returns the result.
   The existing value of board->hash is neither used nor updated. */
unsigned long long zobrist_hash(const Board *board);
#endif

/* Do/undo moves (which must be valid, e.g. returned by generate_moves()) */
void board_do(Board *board, const Move *m);
void board_undo(Board *board, const Move *m);

/* Does an internal consistency check on the board and aborts the program if
   any check fails. Does not catch all inconsistencies! */
#ifndef NDEBUG
void board_validate(const Board *board);
#else /* def NDEBUG */
#define board_validate(board)
#endif

/* Generates a list of all moves for both players and returns it length.
   This list does not include passes for either player. */
int generate_all_moves(const Board *board, Move moves[2*M]);

/* Generates a list of moves for the current player and returns its length.
   If and only if the player has no stacking moves, the result includes a pass
   move, so the result is at least 1. */
int generate_moves(const Board *board, Move moves[M]);

/* Determines if the given `board' allows the current player to play `move'.
   Note that this is a relatively costly function because it first generates
   all possible moves! (This is necessary to check if passing is allowed.) */
bool valid_move(const Board *board, const Move *move);

/* Calculates the score for both players: */
void board_scores(const Board *board, int scores[2]);

/* Calculates the score for the current player: */
int board_score(const Board *board);

#endif /* ndef GAME_H_INCLUDED */
