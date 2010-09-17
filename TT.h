#ifndef TT_H_INCLUDED

#include "Game.h"

/* Number of entries in the transposition table: */
#define TT_SIZE 3000007

/* 64-bit hash code type: */
typedef unsigned long long hash_t;

/* Transposition table entry: */
typedef struct TTEntry {
	hash_t hash;
	int value;
} TTEntry;

/* Transposition table: */
extern TTEntry tt[TT_SIZE];

/* Serializes the board into a unique 50-character representation: */
EXTERN void serialize_board(const Board *board, unsigned char data[50]);

/* Computes the FNV1 hash code for a binary string: */
EXTERN hash_t fnv1(const unsigned char *buf, int len);

/* Computes a hash code for the given board. */
EXTERN hash_t hash_board(const Board *board);

#endif /* ndef TT_H_INCLUDED */
