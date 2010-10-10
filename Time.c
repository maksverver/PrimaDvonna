#include "Time.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>

static double limit, used, start;

/* Alarm data: */
static struct sigaction old_action;
static void (*callback_func)(void *);
static void *callback_arg;

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

static void alarm_handler(int signum)
{
	(void)signum;  /* ignored */

	callback_func(callback_arg);
}

EXTERN void set_alarm(double time, void(*callback)(void*), void *arg)
{
	struct itimerval new_timer, old_timer;
	struct sigaction new_action;

	/* Record callback handler function and argument: */
	callback_func = callback;
	callback_arg  = arg;

	/* Install signal handler */
	new_action.sa_handler = alarm_handler;
	sigemptyset(&new_action.sa_mask);
	new_action.sa_flags   = 0;
	sigaction(SIGALRM, &new_action, &old_action);

	/* Set timer with setitimer */
	new_timer.it_interval.tv_sec  = 0;
	new_timer.it_interval.tv_usec = 0;
	new_timer.it_value.tv_sec  = (long)floor(time);
	new_timer.it_value.tv_usec = (long)floor((time - floor(time))*1e6);
	setitimer(ITIMER_REAL, &new_timer, &old_timer);
	assert(old_timer.it_value.tv_sec == 0 && old_timer.it_value.tv_usec == 0);
}

EXTERN void clear_alarm()
{
	struct itimerval stop_timer = { { 0, 0 }, { 0, 0 } };
	setitimer(ITIMER_REAL, &stop_timer, NULL);
	sigaction(SIGALRM, &old_action, NULL);
	callback_func = NULL;
	callback_arg  = NULL;
}
