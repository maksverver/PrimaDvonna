#ifndef GAME_H_INCLUDED
#define GAME_H_INCLUDED

#include <stdbool.h>
#include <stdlib.h>

#define W    11             /* board width */
#define H     5             /* board height */
#define N    49             /* maximum number of accessible fields */
#define D     3             /* nubmer of Dvonn pieces */
#define M ((N-D)/2*6)       /* maximum number of moves */

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

/* Board creation/modification functions: */
void board_clear(Board *board);
void board_place(Board *board, const Place *p);
void board_unplace(Board *board, const Place *p);
void board_move(Board *board, const Move *m, Color *old_player);
void board_unmove(Board *board, const Move *m, Color old_player);
void board_validate(const Board *board);

/* Generates a list of possible placements and returns its length: */
int generate_places(const Board *board, Place places[N]);

/* Generates all moves for both players: */
void generate_all_moves( const Board *board,
	Move *moves1, Move *moves2, int *nmove1, int *nmove2 );

#define next_player(b) ((b)->moves < N ? ((b)->moves&1) : ((b)->moves - N)&1)

/* Generates a list of moves for the current player and returns its length: */
int generate_moves(const Board *board, Move moves[M]);

/* Calculates the score for both players: */
void board_scores(const Board *board, int scores[2]);

/* Calculates the score for the current player: */
int board_score(const Board *board);

#endif /* ndef GAME_H_INCLUDED */
