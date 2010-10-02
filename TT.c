#include "TT.h"
#include <assert.h>

TTEntry tt[TT_SIZE];

/* Encodes the board into a single byte for the game phase and current player,
   followed by 49 bytes, one for each field, encoding the fields as:
     (dvonns ? 128 : 0) + (player != NONE ? 2*pieces + player : 0) */
EXTERN void serialize_board(const Board *board, unsigned char data[50])
{
	const Field *f = &board->fields[0][0];
	unsigned char *p = data;
	int n;

	*p++ = (board->moves < N ? 0 : 2) + board->moves%2;
	for (n = H*W; n > 0; --n) {
		if (f->removed != (unsigned char)-1) {
			*p = 0;
			if (!f->removed) {
				if (f->dvonns) *p += 128;
				if (f->player != NONE) *p += 2*f->pieces + f->player;
			}
			++p;
		}
		++f;
	}
	assert(p == data + 50);
}

EXTERN hash_t fnv1(const unsigned char *buf, int len)
{
	hash_t res = 14695981039346656037ULL;
	for (; len > 0; --len) {
		res *= 1099511628211ULL;
		res ^= *buf++;
	}
	return res;
}

EXTERN hash_t hash_board(const Board *board)
{
	unsigned char data[50];
	serialize_board(board, data);
	return fnv1(data, 50);
}

EXTERN void tt_store(const Board *board, short depth, val_t lo, val_t hi)
{
	hash_t hash = hash_board(board);
	TTEntry *entry = &tt[hash%TT_SIZE];

	entry->hash  = hash;
	entry->lo    = lo;
	entry->hi    = hi;
	entry->depth = depth;
}

EXTERN bool tt_fetch(const Board *board, short min_depth, val_t *lo, val_t *hi)
{
	hash_t hash = hash_board(board);
	TTEntry *entry = &tt[hash%TT_SIZE];

	if (entry->hash != hash || entry->depth < min_depth) return false;
	*lo = entry->lo;
	*hi = entry->hi;
	return true;
}
