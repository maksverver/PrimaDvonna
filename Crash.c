#include "Crash.h"
#include <sys/signal.h>
#include <stdio.h>
#include <string.h>

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

	if (!name) name = "Unknown";
	fprintf(stderr, "Crash handler caught signal %d: %s!\n", signal, name);
	crash_fini();
	raise(signal);
}

EXTERN void crash_init()
{
	struct sigaction sa;
	int i;

	sa.sa_handler = crash_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;  /* FIXME: use alternate stack? */

	for (i = 0; signals[i]; ++i) {
		if (sigaction(signals[i], &sa, NULL) != 0) {
			fprintf(stderr, "Couldn't install signal handler %d!\n", i);
		}
	}
}

EXTERN void crash_fini()
{
	struct sigaction sa;
	int i;

	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	for (i = 0; signals[i]; ++i) {
		/* Don't check return code here. If this fails we're boned anyway. */
		sigaction(signals[i], &sa, NULL);
	}
}
