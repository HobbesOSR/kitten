#include <lwk/kernel.h>
#include <lwk/time.h>
#include <arch/uaccess.h>

int
sys_gettimeofday(
	struct timeval  __user * tv,
	struct timezone __user * tz
)
{
	struct timeval  _tv;
	struct timezone _tz;

	if (tv != NULL) {
		uint64_t now = get_time(); /* nanoseconds */

		_tv.tv_sec  = now / NSEC_PER_SEC;
		_tv.tv_usec = (now % NSEC_PER_SEC) / USEC_PER_NSEC;

		if (copy_to_user(tv, &_tv, sizeof(_tv)))
			return -EFAULT;
	}

	if (tz != NULL) {
		_tz.tz_minuteswest = 0;
		_tz.tz_dsttime     = 0;

		if (copy_to_user(tz, &_tz, sizeof(_tz)))
			return -EFAULT;
	}
	
	return 0;
}

