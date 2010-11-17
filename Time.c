#include "Time.h"
#include "Signal.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/time.h>

static bool started;
static struct timeval used, start;
double time_limit;

/* Alarm data: */
static signal_handler_t old_handler;
static void (*callback_func)(void *);
static void *callback_arg;

void time_restart(void)
{
	used.tv_sec  = 0;
	used.tv_usec = 0;
	gettimeofday(&start, NULL);
	started = true;
}

void time_start(void)
{
	if (!started) {
		gettimeofday(&start, NULL);
		started = true;
	}
}

void time_stop(void)
{
	if (started) {
		struct timeval now;
		gettimeofday(&now, NULL);
		used.tv_sec  += now.tv_sec - start.tv_sec;
		used.tv_usec += now.tv_usec - start.tv_usec;
		if (used.tv_usec < 0) {
			--used.tv_sec;
			used.tv_usec += 1000000;
		} else if (used.tv_usec >= 1000000) {
			++used.tv_sec;
			used.tv_usec -= 1000000;
		}
		started = false;
	}
}

double time_used(void)
{
	struct timeval res = used;
	if (started) {
		struct timeval now;
		gettimeofday(&now, NULL);
		res.tv_sec  += now.tv_sec - start.tv_sec;
		res.tv_usec += now.tv_usec - start.tv_usec;
		/* N.B. no need to normalize `res' */
	}
	return res.tv_sec + 1e-6*res.tv_usec;
}

double time_left(void)
{
	return time_limit - time_used();
}

static void alarm_callback(int signum)
{
	(void)signum;  /* ignored */

	callback_func(callback_arg);
}

void set_alarm(double time, void(*callback)(void*), void *arg)
{
	struct itimerval new_timer, old_timer;
	signal_handler_t new_handler;

	/* Record callback handler function and argument: */
	callback_func = callback;
	callback_arg  = arg;

	/* Install signal handler */
	signal_handler_init(&new_handler, alarm_callback);
	signal_swap_handlers(SIGALRM, &new_handler, &old_handler);

	/* Set timer with setitimer */
	new_timer.it_interval.tv_sec  = 0;
	new_timer.it_interval.tv_usec = 0;
	new_timer.it_value.tv_sec  = (time_t)floor(time);
	new_timer.it_value.tv_usec = (suseconds_t)floor((time - floor(time))*1e6);
	setitimer(ITIMER_REAL, &new_timer, &old_timer);
	assert(old_timer.it_value.tv_sec == 0 && old_timer.it_value.tv_usec == 0);
}

void clear_alarm(void)
{
	struct itimerval stop_timer = { { 0, 0 }, { 0, 0 } };
	setitimer(ITIMER_REAL, &stop_timer, NULL);
	signal_swap_handlers(SIGALRM, &old_handler, NULL);
	callback_func = NULL;
	callback_arg  = NULL;
}
