#ifndef TT_H_INCLUDED

#include "Game.h"
#include "AI.h"

/* Number of entries in the transposition table: */
#define TT_SIZE 3000007

/* 64-bit hash code type: */
typedef unsigned long long hash_t;

/* Transposition table entry: */
typedef struct TTEntry {
	hash_t hash;
	val_t  lo, hi;
	short  depth;
} TTEntry;

/* Transposition table: */
extern TTEntry tt[TT_SIZE];

/* Update the bounds for the given position in the transposition table. */
EXTERN void tt_store(const Board *board, short depth, val_t lo, val_t hi);

/* Retrieve the bounds for the given position from the transposition table.
   Returns false if the position was not found (with the required depth). */
EXTERN bool tt_fetch(const Board *board, short min_depth, val_t *lo, val_t *hi);

/* Serializes the board into a unique 50-character representation: */
EXTERN void serialize_board(const Board *board, unsigned char data[50]);

/* Computes the FNV1 hash code for a binary string: */
EXTERN hash_t fnv1(const unsigned char *buf, int len);

/* Computes a hash code for the given board. */
EXTERN hash_t hash_board(const Board *board);

#endif /* ndef TT_H_INCLUDED */
