#include <lwk/task.h>
#include <lwk/signal.h>
#include <lwk/ptrace.h>

/**
 * Determines if a signal is unhandled.
 * Returns 1 if the signal is unhandled, 0 otherwise.
 */
int
unhandled_signal(struct task_struct *tsk, int sig)
{
	if (is_init(tsk))
		return 1;
	if (tsk->ptrace & PT_PTRACED)
		return 0;
	return (tsk->sighand->action[sig-1].sa.sa_handler == SIG_IGN) ||
		(tsk->sighand->action[sig-1].sa.sa_handler == SIG_DFL);
}

