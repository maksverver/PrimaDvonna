#ifndef TT_H_INCLUDED

#include "Game.h"
#include "AI.h"

/* 64-bit hash code type: */
typedef unsigned long long hash_t;

/* Transposition table entry: */
typedef struct TTEntry {
	hash_t hash;
	val_t  lo, hi;
	short  depth;
#ifdef TT_DEBUG
	unsigned char data[50];
#endif
} TTEntry;

/* Transposition table: */
extern TTEntry *tt;
extern size_t tt_size;

/* Allocates a transposition table with `size' entries. */
EXTERN void tt_alloc(size_t size);

/* Frees the currently allocated transposition table. */
EXTERN void tt_free();

/* Serializes the board state into a unique 50-byte descriptor. */
EXTERN void serialize_board(const Board *board, unsigned char output[50]);

/* Computes a 64-bit FNV-1 hash. */
EXTERN hash_t fnv1(unsigned char *data, size_t len);

/* Computes a hash code for the given board. */
EXTERN hash_t hash_board(const Board *board);

#endif /* ndef TT_H_INCLUDED */
