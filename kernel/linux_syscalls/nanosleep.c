#include <lwk/kernel.h>
#include <lwk/time.h>
#include <lwk/timer.h>
#include <arch/uaccess.h>

int
sys_nanosleep(const struct timespec __user *req, struct timespec __user *rem)
{
	struct timespec _req, _rem;
	uint64_t when, remain;

	if (copy_from_user(&_req, req, sizeof(_req)))
		return -EFAULT;

	if (!timespec_is_valid(&_req))
		return -EINVAL;

	when = get_time() + (_req.tv_sec * NSEC_PER_SEC) + _req.tv_nsec;
	remain = timer_sleep_until(when);

	if (remain && rem) {
		_rem.tv_sec  = remain / NSEC_PER_SEC;
		_rem.tv_nsec = remain % NSEC_PER_SEC;

		if (copy_to_user(rem, &_rem, sizeof(_rem)))
			return -EFAULT;
	}

	return (remain) ? -ERESTART_RESTARTBLOCK : 0;
}
