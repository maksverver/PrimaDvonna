#ifndef TIME_H_INCLUDED
#define TIME_H_INCLUDED

extern double time_limit;

/* Resets the timer, and starts it. */
void time_restart(void);

/* Resumes the timer, if it is paused. */
void time_start(void);

/* Pauses the timer, if it is running. */
void time_stop(void);

/* Returns how much time has been used; i.e. for how long the timer was running
   since the last reset, not counting the time it was paused.*/
double time_used(void);

/* Returns how much time is left; which is basically time_limit - time_used() */
double time_left(void);

/* Schedules the given callback function to be called after `time' seconds.
   Only one alarm can be active at any given time! */
void set_alarm(double time, void(*callback)(void *arg), void *arg);

/* Cancels the current alarm, if any is currently set. Note that the alarm may
   still go off before this function returns! */
void clear_alarm(void);

#endif /* ndef TIME_H_INCLUDED */
