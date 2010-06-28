#include <lwk/kernel.h>
#include <lwk/signal.h>
#include <arch/uaccess.h>

long
sys_rt_sigpending(
	sigset_t __user *	set,
	size_t			sigsetsize
)
{
	sigset_t _set;
	int status;

	if (sigsetsize > sizeof(sigset_t))
		return -EINVAL;

	status = sigpending(&_set);
	if (status)
		return status;

	if (copy_to_user(set, &_set, sigsetsize))
		return -EFAULT;

	return 0;
}
