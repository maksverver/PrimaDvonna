#ifndef GAME_H_INCLUDED
#define GAME_H_INCLUDED

#include <stdbool.h>
#include <stdlib.h>

#define W    11             /* board width */
#define H     5             /* board height */
#define N    49             /* maximum number of accessible fields */
#define D     3             /* nubmer of Dvonn pieces */
#define M ((N-D)/2*6)       /* maximum number of moves (for one player) */

typedef enum Color { WHITE = 0, BLACK = 1, NONE = -1 } Color;

/* A description of the contents of a game field.
   See board_validate() for the invariants that apply to this struct's data. */
typedef struct Field
{
	signed char   player;      /* controlling player's color */
	unsigned char pieces;      /* number of pieces on this field */
	unsigned char dvonns;      /* number of dvonn pieces on this field */
	signed char   removed;     /* move on which field was removed */
	signed char   mobile;      /* number of open neighbouring directions */
} Field;

typedef struct Board
{
	int   moves;          /* number of moves played on this board */
	Field fields[H][W];   /* fields of the board */
} Board;

/* This macro determines which color plays next: */
#define next_player(b) ((b)->moves < N ? ((b)->moves&1) : ((b)->moves - N)&1)

/* The structure below describes a single move.
   There are three types of moves: placing, stacking and passing.

   (r1,c1) is the source field when stacking, or the destination field when
   placing, or (-1, -1) when passing. Similarly, (r2,c2) is the destination
   field when stacking, or (-1, -1) when placing or passing. */
typedef struct Move {
	signed char r1, c1, r2, c2;
} Move;

/* Union of a move and an integer. Used for quick comparisons of moves. */
typedef union MoveInt {
	struct Move m;
	int         i;
} MoveInt;

extern Move move_null;  /* null move: all fields set to 0 */
extern Move move_pass;  /* pass move: all fields set to -1 */
#define move_places(m) ((m)->r1 >= 0 && (m)->r2 < 0)
#define move_stacks(m) ((m)->r2 >= 0)
#define move_passes(m) ((m)->r1 < 0)
#define move_is_null(m) (((union MoveInt)*(m)).i == 0)
#define move_compare(a, b) (((union MoveInt)*(a)).i - ((union MoveInt)*(b)).i)

/* The six cardinal directions on the board. These are ordered anti-clockwise
   starting to right. (Order matters for functions like may_be_bridge()!) */
extern const int DR[6], DC[6];

/* Returns the distance between two pairs of field coordinates: */
EXTERN int distance(int r1, int c1, int r2, int c2);

/* (Re)initialize a board structure to an empty board: */
EXTERN void board_clear(Board *board);

/* Do/undo moves (which must be valid, e.g. returned by generate_moves()) */
EXTERN void board_do(Board *board, const Move *m);
EXTERN void board_undo(Board *board, const Move *m);

/* Does an internal consistency check on the board and aborts the program if
   any check fails. Does not catch all inconsistencies! */
EXTERN void board_validate(const Board *board);

/* Generates a list of all moves for both players and returns it length.
   This list does not include passes for either player. */
EXTERN int generate_all_moves(const Board *board, Move moves[2*M]);

/* Generates a list of moves for the current player and returns its length.
   If and only if the player has no stacking moves, the result includes a pass
   move, so the result is at least 1. */
EXTERN int generate_moves(const Board *board, Move moves[M]);

/* Determines if the given `board' allows the current player to play `move'.
   Note that this is a relatively costly function because it first generates
   all possible moves! (This is necessary to check if passing is allowed.) */
EXTERN bool valid_move(const Board *board, const Move *move);

/* Calculates the score for both players: */
EXTERN void board_scores(const Board *board, int scores[2]);

/* Calculates the score for the current player: */
EXTERN int board_score(const Board *board);

/* Returns an upper bound on the number of moves left in the game: */
EXTERN int max_moves_left(const Board *board);

#endif /* ndef GAME_H_INCLUDED */
