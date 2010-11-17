#include "Crash.h"
#include "Signal.h"
#include <stdio.h>
#include <unistd.h>
#include <execinfo.h>
extern char *strsignal(int sig);

static int signals[] = {
	/* Signals which cause the process to exit: */
	SIGALRM, SIGHUP, SIGINT, SIGPIPE, SIGTERM,

	/* Signals which cause the process to abort: */
	SIGABRT, SIGFPE, SIGILL, SIGQUIT, SIGSEGV, SIGTRAP, SIGBUS,

	0 };

/* Handler called in case of a crash. Prints the signal number and name to
   stderr, resets all handlers to default, and then reraises the signal,
   causing the program to exit.

   Note that this may fail because calling libc from a signal handler is
   potentially unsafe, and if the stack is messed up we might not be to call
   any functions at all!
*/
static void crash_handler(int signal)
{
	const char *name = strsignal(signal);
	void *ptrs[100];
	int nptr;

	if (!name) name = "Unknown";
	fprintf(stderr, "Crash handler caught signal %d: %s!\n", signal, name);

	fprintf(stderr, "Backtrace:\n");
	nptr = backtrace(ptrs, sizeof(ptrs)/sizeof(*ptrs));
	backtrace_symbols_fd(ptrs, nptr, STDERR_FILENO);
	
	fflush(stderr);
	crash_fini();
	raise(signal);
}

void crash_init(void)
{
	signal_handler_t handler;
	int i;

	signal_handler_init(&handler, crash_handler);
	for (i = 0; signals[i]; ++i) {
		if (signal_swap_handlers(signals[i], &handler, NULL) != 0) {
			fprintf(stderr, "Couldn't install signal handler %d!\n", i);
		}
	}
}

void crash_fini(void)
{
	signal_handler_t handler;
	int i;

	signal_handler_init(&handler, SIG_DFL);
	for (i = 0; signals[i]; ++i) {
		/* Don't check return code here. If this fails we're boned anyway. */
		signal_swap_handlers(signals[i], &handler, NULL);
	}
}
