#ifndef TT_H_INCLUDED

#include "Game.h"
#include "AI.h"

typedef unsigned long long hash_t;

/* Transposition table entry: */
typedef struct TTEntry {
	hash_t hash;
	val_t  lo, hi;
	int    depth;
	Move   killer;
#ifdef TT_DEBUG
	unsigned char data[50];
#endif
} TTEntry;

/* Transposition table: */
extern TTEntry *tt;
extern size_t tt_size;

/* Allocates a transposition table with `size' entries. */
EXTERN void tt_init(size_t size);

/* Frees the currently allocated transposition table. */
EXTERN void tt_fini(void);

/* Serializes the board state into a unique 50-byte descriptor. */
EXTERN void serialize_board(const Board *board, unsigned char output[50]);

#ifdef ZOBRIST
#define hash_board(board) ((hash_t)((board)->hash.ull))
#else
/* Computes a hash code for the given board. */
EXTERN hash_t hash_board(const Board *board);
#endif

#endif /* ndef TT_H_INCLUDED */
