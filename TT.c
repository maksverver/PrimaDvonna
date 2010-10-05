#include "TT.h"
#include <assert.h>

TTEntry tt[TT_SIZE];

#define FNV64_OFFSET_BASIS 14695981039346656037ULL
#define FNV64_PRIME 1099511628211ULL

EXTERN hash_t hash_board(const Board *board)
{
	const Field *f, *g;
	hash_t res = FNV64_OFFSET_BASIS;

	/* Encode game phase and next player: */
	res *= FNV64_PRIME;
	res ^= (board->moves < N ? 0 : 2) + board->moves%2;

	/* Encode fields: */
	for (f = &board->fields[0][0], g = f + H*W; f != g; ++f) {
		if (f->removed != (unsigned char)-1) {
			res *= FNV64_PRIME;
			if (!f->removed) {
				if (f->dvonns) res ^= 128;
				if (f->player != NONE) res ^= 2*f->pieces + f->player;
			}
		}
	}

	return res;
}
