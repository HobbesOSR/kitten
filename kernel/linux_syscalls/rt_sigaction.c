#include <lwk/kernel.h>
#include <lwk/signal.h>

long
sys_rt_sigaction(
	int				sig,
	const struct sigaction __user *	act,
	struct sigaction __user *	oact,
	size_t				sigsetsize
)
{
	/*
 	 * TODO: Actually do something.
 	 *       We're faking this out so glibc's sleep() works.
 	 */
	return 0;
}
