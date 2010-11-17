#ifndef MOVE_ORDERING_H_INCLUDED
#define MOVE_ORDERING_H_INCLUDED

#include "Game.h"

/* Shuffle moves randomly using rand() */
void shuffle_moves(Move *moves, int nmove);

/* Moves the given killer move to the front of the list (if it is found) and
   leaves all other moves in the same order. */
void move_to_front(Move *moves, int nmove, Move killer);

/* Heuristically (but quickly) orders moves from best-to-worst. */
void order_moves(const Board *board, Move *moves, int nmove);

#endif /* ndef MOVE_ORDERING_H_INCLUDED */
