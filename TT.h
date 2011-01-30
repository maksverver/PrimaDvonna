#ifndef TT_H_INCLUDED

#include "Game.h"
#include "AI.h"

/* Transposition table entry.
   (See dfs() in AI.c for the interpretation of these fields.) */
typedef struct TTEntry {
	hash_t hash;         /* full hash code of board (0 for empty entries) */
	val_t  lo, hi;       /* game value is be between lo and hi (inclusive) */
	short  depth;        /* search depth used to determine value */
	short  relevance;    /* used by replacement policy (see dfs() in AI.c) */
	Move   killer;       /* best known move for this position */
#ifdef TT_DEBUG
	unsigned char data[50];
#endif
} TTEntry;

#ifdef TT_DEBUG
typedef struct TTStats {
	long long queries;      /* number of times the TT was queried */
	long long missing;      /*   of which position was missing */
	long long shallow;      /*   of which search depth was insufficient */
	long long partial;      /*   which were used only partially */
	long long updates;      /* number of updates */
	long long discarded;    /*   of which discarded due to replacement policy */
	long long updated;      /*   position existed with equal depth */
	long long upgraded;     /*   position existed with different depth */
	long long overwritten;  /*   different position existed */
} TTStats;

TTStats tt_stats;
#endif

/* Transposition table: */
extern TTEntry *tt;
#ifdef FIXED_PARAMS
#define tt_size ((size_t)1 << ai_use_tt)
#else
extern size_t tt_size;
#endif

/* Allocates a transposition table with `size' entries. */
void tt_init(size_t size);

/* Frees the currently allocated transposition table. */
void tt_fini(void);

/* Serializes the board state into a unique 50-byte descriptor. */
void serialize_board(const Board *board, unsigned char output[50]);

#ifdef ZOBRIST
#define hash_board(board) ((board)->hash)
#else
/* Computes a hash code for the given board. */
hash_t hash_board(const Board *board);
#endif

#ifdef TT_DEBUG
/* Counts the number of valid entries in the transposition table. */
size_t tt_population_count(void);
#endif

#endif /* ndef TT_H_INCLUDED */
