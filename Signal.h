#ifndef SIGNAL_H_INCLUDED
#define SIGNAL_H_INCLUDED

/* Convenience functions for installing/uninstalling signal handlers.
   Currently defined macros are intended to work on Linux with GCC. */

#include <signal.h>

typedef struct sigaction signal_handler_t;

#define signal_handler_init(handler_ptr, callback_func) \
	({ *handler_ptr = (struct sigaction){ .sa_handler = callback_func}; })
#define signal_swap_handlers(signal_number, new_handler_ptr, old_handler_ptr) \
	sigaction(signal_number, new_handler_ptr, old_handler_ptr)

#endif /* ndef SIGNAL_H_INCLUDED */
