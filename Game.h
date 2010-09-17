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
	signed char   player;  /* controlling player's color */
	unsigned char pieces;  /* number of pieces on this field */
	unsigned char dvonns;  /* number of dvonn pieces on this field */
	unsigned char removed; /* move on which field was removed */
} Field;

typedef struct Board
{
	int   moves;          /* number of moves played on this board */
	Field fields[H][W];   /* fields of the board */
} Board;

/* Describes the placing of a piece on the given field: */
typedef struct Place { unsigned char r, c; } Place;

/* Describes the moving of a stack unto another stack: */
typedef struct Move { unsigned char r1, c1, r2, c2; } Move;

extern Move move_pass;
#define move_is_pass(m) ((m)->r1 == (unsigned char)-1)

/* Returns the distance between two pairs of field coordinates: */
EXTERN int distance(int r1, int c1, int r2, int c2);

/* Returns whether the given field lies on the edge of the board: */
EXTERN bool is_edge_field(const Board *board, int r, int c);

/* Board creation/modification functions: */
EXTERN void board_clear(Board *board);
EXTERN void board_place(Board *board, const Place *p);
EXTERN void board_unplace(Board *board, const Place *p);
EXTERN void board_move(Board *board, const Move *m, Color *old_player);
EXTERN void board_unmove(Board *board, const Move *m, Color old_player);
EXTERN void board_validate(const Board *board);

/* Generates a list of possible placements and returns its length: */
EXTERN int generate_places(const Board *board, Place places[N]);

/* Generates a list of all moves for both players and returns it length.
   This list does not include passes for either player. */
EXTERN int generate_all_moves(const Board *board, Move moves[2*M]);

#define next_player(b) ((b)->moves < N ? ((b)->moves&1) : ((b)->moves - N)&1)

/* Generates a list of moves for the current player and returns its length.
   If and only if the player has no stacking moves, the result includes a pass
   move, so the result is at least 1. */
EXTERN int generate_moves(const Board *board, Move moves[M]);

/* Calculates the score for both players: */
EXTERN void board_scores(const Board *board, int scores[2]);

/* Calculates the score for the current player: */
EXTERN int board_score(const Board *board);

#endif /* ndef GAME_H_INCLUDED */
