#include <lwk/kernel.h>
#include <lwk/signal.h>

long
sys_rt_sigprocmask(
	int			how,
	sigset_t __user *	set,
	sigset_t __user *	oset,
	size_t			sigsetsize
)
{
	/*
 	 * TODO: Actually do something.
 	 *       We're faking this out so glibc's sleep() works.
 	 */
	return 0;
}
