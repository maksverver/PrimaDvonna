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

/* Computes a hash code for the given board. */
EXTERN hash_t hash_board(const Board *board);

#endif /* ndef TT_H_INCLUDED */
