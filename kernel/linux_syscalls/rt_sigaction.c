#include <lwk/kernel.h>
#include <lwk/signal.h>
#include <arch/uaccess.h>

long
sys_rt_sigaction(
	int				sig,
	const struct sigaction __user *	act,
	struct sigaction __user *	oact,
	size_t				sigsetsize
)
{
	struct sigaction _act, _oact;
	int status;

	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if ((sig == SIGKILL) || (sig == SIGSTOP))
		return -EINVAL;

	if (act) {
		if (copy_from_user(&_act, act, sizeof(_act)))
			return -EFAULT;

		// Per-POSIX, user-space can not block SIGKILL or SIGSTOP
		sigset_del(&_act.sa_mask, SIGKILL);
		sigset_del(&_act.sa_mask, SIGSTOP);
	}

	status = sigaction(sig, act ? &_act : NULL, oact ? &_oact : NULL);
	if (status)
		return status;

	if (oact) {
		if (copy_to_user(oact, &_oact, sizeof(_oact)))
			return -EFAULT;
	}

	return 0;
}
