#ifndef TIME_H_INCLUDED
#define TIME_H_INCLUDED

extern double time_limit;

void time_restart(void);
void time_start(void);
void time_stop(void);
double time_used(void);
double time_left(void);
void set_alarm(double time, void(*callback)(void *arg), void *arg);
void clear_alarm(void);

#endif /* ndef TIME_H_INCLUDED */
