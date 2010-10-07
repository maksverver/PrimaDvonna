#ifndef TIME_H_INCLUDED
#define TIME_H_INCLUDED

EXTERN void time_restart();
EXTERN void time_start();
EXTERN void time_stop();
EXTERN double time_used();
EXTERN double time_left();
EXTERN void set_alarm(double time, void(*callback)());
EXTERN void clear_alarm();

#endif /* ndef TIME_H_INCLUDED */
