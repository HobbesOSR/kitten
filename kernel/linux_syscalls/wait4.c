#include <lwk/kernel.h>
#include <lwk/aspace.h>
#include <lwk/waitq.h>

#define WNOHANG 0x00000001

long
sys_wait4(
	pid_t		pid,
	int __user *	exit_status,
	int		options
)
{
	int status;
	id_t _child_id;
	bool _block;
	id_t _exit_id;
	int  _exit_status;

	if (pid == -1)
		_child_id = ANY_ID;
	else if (pid == 0)
		_child_id = MY_ID;
	else if ((pid >= UASPACE_MIN_ID) && (pid <= UASPACE_MAX_ID))
		_child_id = pid;
	else
		return -EINVAL;

	_block = (options & WNOHANG) ? false : true;

	status = aspace_wait4_child_exit(_child_id, _block,
					 &_exit_id, &_exit_status);

	if (status == -EAGAIN) {
		// The call would have blocked, but there are one or more
		// non-exited children so a future call may succeed.
		return 0;
	} else if (status == -EINTR) {
		// Fixup so that kernel will automatically restart the system
		// call after any pending signals are handled. User-space will
		// never see the -ERESTARTSYS return code. 
		return -ERESTARTSYS;
	} else if (status != 0) {
		// Some random error returned, pass straight through to caller
		return status;
	}

	// Success, return the exited pid and its exit status to caller

	if (exit_status) {
		if (copy_to_user(exit_status, &_exit_status, sizeof(_exit_status)))
			return -EFAULT;
	}

	return _exit_id;
}
