#ifndef IO_H_INCLUDED
#define IO_H_INCLUDED

#include "Game.h"

bool parse_place(const char *text, Place *place);
bool parse_move(const char *text, Move *move);
bool parse_state(const char *descr, Board *board, Color *next_player);

const char *format_place(const Place *place);
const char *format_move(const Move *move);
const char *format_state(Board *board);

#endif /* ndef IO_H_INCLUDED */
