#ifndef IO_H_INCLUDED
#define IO_H_INCLUDED

#include "Game.h"

EXTERN bool parse_move(const char *text, Move *move);
EXTERN bool parse_state(const char *descr, Board *board, Color *next_player);

EXTERN const char *format_move(const Move *move);
EXTERN const char *format_state(const Board *board);

#endif /* ndef IO_H_INCLUDED */
