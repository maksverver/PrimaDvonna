#ifndef IO_H_INCLUDED
#define IO_H_INCLUDED

#include "Game.h"

/* Parses a move string (e.g. "PASS", "A1", "A1B2", etc.) and assigns the result
   to *move, or returns false if the input is not properly formatted. */
bool parse_move(const char *text, Move *move);

/* Parses a board description, and assigns the result to `*board', as well as
   the player to move next (if any) to `*next_player', or returns false if the
   board description is invalid (see ENCODING.txt for details). */
bool parse_state(const char *descr, Board *board, Color *next_player);

/* Returns a string representation of the given move. The result should not be
   freed by the caller; it remains valid until the next call to format_move() */
const char *format_move(const Move *move);

/* Returns a string representation of the given board. The result should not be
   freed by the caller; it remains valid until the next call to format_move() */
const char *format_state(const Board *board);

#endif /* ndef IO_H_INCLUDED */
