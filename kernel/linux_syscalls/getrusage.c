#include <lwk/kernel.h>
#include <lwk/rlimit.h>
#include <arch/uaccess.h>

#define RUSAGE_SELF     0

struct rusage
{
	struct timeval ru_utime;
	struct timeval ru_stime;
};

long
sys_getrusage(
	unsigned int		who,
	struct rusage __user *	ru
)
{
	struct rusage _ru;
	struct timeval _tv;

	uint64_t now = get_time(); /* nanoseconds */
	_tv.tv_sec  = now / NSEC_PER_SEC;
	_tv.tv_usec = (now % NSEC_PER_SEC) / NSEC_PER_USEC;

	switch (who) {
		case RUSAGE_SELF:
			_ru.ru_utime = _tv;
			break;
		default:
			printk("Unsupported rusage (who=%d).\n", who);
			return -EINVAL;
	}

	if (copy_to_user(ru, &_ru, sizeof(struct rusage)))
		return -EFAULT;

	return 0;
}
