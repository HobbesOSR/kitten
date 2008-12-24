#include <lwk/kernel.h>
#include <lwk/time.h>
#include <lwk/timer.h>
#include <arch/uaccess.h>

int
sys_nanosleep(const struct timespec __user *req, struct timespec __user *rem)
{
	struct timespec _req;

	if (copy_from_user(&_req, req, sizeof(_req)))
		return -EFAULT;

	if (!timespec_is_valid(&_req))
		return -EINVAL;

	uint64_t when = get_time()
		+ (_req.tv_sec * NSEC_PER_SEC)
		+ _req.tv_nsec;

	uint64_t remain = timer_sleep_until(when);
	if( !remain )
		return 0;
	if( !rem )
		return -ERESTART_RESTARTBLOCK;

	struct timespec _rem = {
		.tv_sec  = remain / NSEC_PER_SEC,
		.tv_nsec = remain % NSEC_PER_SEC,
	};

	if (copy_to_user(rem, &_rem, sizeof(_rem)))
		return -EFAULT;

	return -ERESTART_RESTARTBLOCK;
}
