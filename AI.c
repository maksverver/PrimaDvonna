#include "AI.h"
#include <assert.h>

bool ai_select_place(Board *board, Place *place)
{
	Place places[N];
	int nplace = generate_places(board, places);
	if (nplace <= 0) return false;
	*place = places[rand()%nplace];
	return true;
}

bool ai_select_move(Board *board, Move *move)
{
	Move moves[M];
	int nmove = generate_moves(board, moves);
	if (nmove <= 0) return false;
	*move = moves[rand()%nmove];
	return true;
}
