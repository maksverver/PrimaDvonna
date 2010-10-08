#include <stdio.h>
#include <assert.h>

#define W    11             /* board width */
#define H     5             /* board height */

const int DR[6] = {  0, -1, -1,  0, +1, +1 };
const int DC[6] = { +1,  0, -1, -1,  0, +1 };

static int min(int i, int j) { return i < j ? i : j; }
static int max(int i, int j) { return i > j ? i : j; }

/* Old function, believed to be wrong (turns out it is right). */
int distance1(int r1, int c1, int r2, int c2)
{
	int dx = c2 - c1;
	int dy = r2 - r1;
	int dz = dx - dy;
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;
	if (dz < 0) dz = -dz;
	return max(max(dx, dy), dz);
}

/* New function, believed to be right (and is right). */
int distance2(int r1, int c1, int r2, int c2)
{
	int dx = c2 - c1;
	int dy = r2 - r1;
	int dz = dx - dy;
	if (dx < 0) dx = -dx;
	if (dy < 0) dy = -dy;
	if (dz < 0) dz = -dz;
	return min(min(dx + dy, dx + dz), dy + dz);
}

int main()
{
	int r1, c1, r2, c2, d;
	int dist[H][W];
	int qr[H*W], qc[H*W], qsize, qpos;

	for (r1 = 0; r1 < H; ++r1) {
		for (c1 = 0; c1 < W; ++c1) {
			/* Breadth-first search to put distance to (r1,c1) in dist: */
			for (r2 = 0; r2 < H; ++r2) {
				for (c2 = 0; c2 < W; ++c2) dist[r2][c2] = -1;
			}
			dist[r1][c1] = 0;
			qpos = 0;
			qsize = 1;
			qr[0] = r1;
			qc[0] = c1;
			while (qpos < qsize) {
				for (d = 0; d < 6; ++d) {
					r2 = qr[qpos] + DR[d];
					c2 = qc[qpos] + DC[d];
					if (r2 >= 0 && r2 < H && c2 >= 0 && c2 < W &&
						dist[r2][c2] == -1) {
						dist[r2][c2] = dist[qr[qpos]][qc[qpos]] + 1;
						qr[qsize] = r2;
						qc[qsize] = c2;
						++qsize;
					}
				}
				++qpos;
			}
			/* Actual verification goes here: */
			for (r2 = 0; r2 < H; ++r2) {
				for (c2 = 0; c2 < W; ++c2) {
					assert(dist[r2][c2] >= 0);
					printf("(%2d,%2d)-(%2d,%2d) => %d\n",
						r1, c1, r2, c2, dist[r2][c2]);
					assert(distance1(r1, c1, r2, c2) == dist[r2][c2]);
					assert(distance2(r1, c1, r2, c2) == dist[r2][c2]);
				}
			}
		}
	}
	return 0;
}
