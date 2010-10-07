#include "Time.h"
#include <math.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

static double limit, used, start;
static struct itimerval old_timer;
static struct sigaction old_action;

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

EXTERN void set_alarm(double time, void(*callback)())
{
	struct itimerval new_timer;
	struct sigaction new_action;

	/* Install signal handler */
	new_action.sa_handler = (void(*)(int))callback;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags   = 0;
	sigaction(SIGALRM, &new_action, &old_action);

	/* Set timer with setitimer */
	new_timer.it_interval.tv_sec  = 1;
	new_timer.it_interval.tv_usec = 0;
	new_timer.it_value.tv_sec  = (long)trunc(time);
	new_timer.it_value.tv_usec = (long)floor((time - trunc(time))*1e6);
	setitimer(ITIMER_REAL, &new_timer, &old_timer);
}

EXTERN void clear_alarm()
{
	setitimer(ITIMER_REAL, &old_timer, NULL);
	sigaction(SIGALRM, &old_action, NULL);
}
