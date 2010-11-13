#include "TT.h"
#include <stdio.h>
#include <assert.h>

TTEntry *tt;
size_t tt_size;

#ifdef TT_DEBUG
TTStats tt_stats;
#endif

EXTERN void tt_init(size_t size)
{
	assert(size > 0);
	assert(tt == NULL);
	assert((size & (size - 1)) == 0);  /* size should be a power of 2 */
	if (size < 1024) size = 1024;
	while (tt == NULL && size >= 1024) {
		tt = calloc(size, sizeof(TTEntry));
		if (tt == NULL) {
			fprintf(stderr, "Faild to allocate %ld bytes for the "
				"transposition table!\n", (long)(size*sizeof(TTEntry)));
			size /= 2;
		}
	}
	tt_size = size;
}

EXTERN void tt_fini(void)
{
	free(tt);
	tt = NULL;
	tt_size = 0;
}

EXTERN void serialize_board(const Board *board, unsigned char output[50])
{
	const Field *f, *g;

	*output++ = (board->moves < N ? 0 : 2) + board->moves%2;
	for (f = &board->fields[0][0], g = f + H*W; f != g; ++f) {
		if (f->removed >= 0) {
			if (!f->removed && f->pieces) {
				int val = 4*f->pieces + 2*f->player;
				if (f->dvonns) ++val;
				*output++ = val;
			} else {
				*output++ = 0;
			}
		}
	}
}

#ifndef ZOBRIST

#define FNV64_OFFSET_BASIS 14695981039346656037ULL
#define FNV64_PRIME 1099511628211ULL

#if 0
static hash_t fnv1(unsigned char *data, size_t len)  /* 64-bit FNV-1 */
{
	hash_t res = FNV64_OFFSET_BASIS;
	for ( ; len > 0; --len) {
		res *= FNV64_PRIME;
		res ^= *data++;
	}
	return res;
}

EXTERN hash_t hash_board(const Board *board)
{
	unsigned char data[50];
	serialize_board(board, data);
	return fnv1(data, 50);
}
#endif

/* This implements a FNV-1 hash of the serialized board, similar to above,
   but manually inlined for speed: */
EXTERN hash_t hash_board(const Board *board)
{
	const Field *f, *g;
	hash_t res = FNV64_OFFSET_BASIS;

	res *= FNV64_PRIME;
	res ^= (board->moves < N ? 0 : 2) + board->moves%2;
	for (f = &board->fields[0][0], g = f + H*W; f != g; ++f) {
		if (f->removed >= 0) {
			res *= FNV64_PRIME;
			if (!f->removed && f->pieces) {
				int val = 4*f->pieces + 2*f->player;
				if (f->dvonns) ++val;
				res ^= val;
			}
		}
	}
	return res;
}

#endif  /* ndef ZOBRIST */

#ifdef TT_DEBUG
EXTERN size_t tt_population_count()
{
	size_t i, res = 0;
	for (i = 0; i < tt_size; ++i) res += tt[i].hash != 0;
	return res;
}
#endif
