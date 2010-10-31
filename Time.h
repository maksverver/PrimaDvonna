#ifndef TIME_H_INCLUDED
#define TIME_H_INCLUDED

extern double time_limit;

EXTERN void time_restart(void);
EXTERN void time_start(void);
EXTERN void time_stop(void);
EXTERN double time_used(void);
EXTERN double time_left(void);
EXTERN void set_alarm(double time, void(*callback)(void *arg), void *arg);
EXTERN void clear_alarm(void);

#endif /* ndef TIME_H_INCLUDED */
