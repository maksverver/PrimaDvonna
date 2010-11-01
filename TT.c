#include "TT.h"
#include <assert.h>

#define FNV64_OFFSET_BASIS 14695981039346656037ULL
#define FNV64_PRIME 1099511628211ULL

TTEntry *tt;
size_t tt_size;

EXTERN void tt_init(size_t size)
{
	assert(size > 0);
	assert(tt == NULL);
	tt_size = size;
	tt = calloc(size, sizeof(TTEntry));
	assert(tt != NULL);
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
			int val = 0;
			if (!f->removed) {
				if (f->dvonns) val = 128;
				if (f->player >= 0) val += 2*f->pieces + f->player;
			}
			*output++ = val;
		}
	}
}

EXTERN hash_t fnv1(unsigned char *data, size_t len)
{
	hash_t res = FNV64_OFFSET_BASIS;
	for ( ; len > 0; --len) {
		res *= FNV64_PRIME;
		res ^= *data++;
	}
	return res;
}

EXTERN hash_t hash_board(const Board *board)  /* 64-bit FNV-1 */
{
	unsigned char data[50];
	serialize_board(board, data);
	return fnv1(data, 50);
}
