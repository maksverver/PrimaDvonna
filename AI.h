#ifndef AI_H_INCLUDED
#define AI_H_INCLUDED

#include "Game.h"

/* Selects the next field to place a piece on during the placement phase. */
EXTERN bool ai_select_place(Board *board, Place *place);

/* Selects the next move to make during the movement phase. */
EXTERN bool ai_select_move(Board *board, Move *move);

#endif /* ndef AI_H_INCLUDED */
