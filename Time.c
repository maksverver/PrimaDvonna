#include "Time.h"
#include <stdlib.h>
#include <sys/time.h>

static double limit, used, start;

static double timestamp(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (double)tv.tv_sec + 1e-6*tv.tv_usec;
}

EXTERN void time_restart(double new_limit)
{
	used  = 0;
	start = timestamp();
	limit = new_limit;
}

EXTERN void time_start()
{
	if (!start) start = timestamp();
}

EXTERN void time_stop()
{
	if (start) used += timestamp() - start;
	start = 0;
}

EXTERN double time_used()
{
	if (start) {
		double now = timestamp();
		used += now - start;
		start = now;
	}
	return used;
}

EXTERN double time_left()
{
	return limit - used;
}
