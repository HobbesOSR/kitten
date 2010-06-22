#include <lwk/kernel.h>
#include <lwk/signal.h>
#include <arch/uaccess.h>

long
sys_rt_sigprocmask(
	int			how,
	const sigset_t __user *	set,
	sigset_t __user *	oldset,
	size_t			sigsetsize
)
{
	sigset_t _set, _oldset;
	int status;

	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if (set) {
		if (copy_from_user(&_set, set, sizeof(sigset_t)))
			return -EFAULT;

		// Per-POSIX, user-space can not block SIGKILL or SIGSTOP
		sigset_del(&_set, SIGKILL);
		sigset_del(&_set, SIGSTOP);
	}

	status = sigprocmask(how, set ? &_set : NULL, oldset ? &_oldset : NULL);
	if (status)
		return status;

	if (oldset) {
		if (copy_to_user(oldset, &_oldset, sizeof(sigset_t)))
			return -EFAULT;
	}

	return 0;
}
