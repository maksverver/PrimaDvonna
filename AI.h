#ifndef AI_H_INCLUDED
#define AI_H_INCLUDED

#include "Game.h"

/* Selects the next move to make during the movement phase. */
EXTERN bool ai_select_move(Board *board, Move *move);

#endif /* ndef AI_H_INCLUDED */
