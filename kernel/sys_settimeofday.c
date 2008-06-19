#include <lwk/kernel.h>
#include <lwk/time.h>
#include <arch/uaccess.h>

int
sys_settimeofday(
	struct timeval  __user * tv,
	struct timezone __user * tz
)
{
	struct timeval  _tv;
	struct timezone _tz;

	if (tv != NULL) {
		if (copy_from_user(&_tv, tv, sizeof(_tv)))
			return -EFAULT;

		set_time(
		  (_tv.tv_sec * NSEC_PER_SEC) + (_tv.tv_usec * NSEC_PER_USEC)
		);
	}

	if (tz != NULL) {
		if (copy_from_user(&_tz, tz, sizeof(_tz)))
			return -EFAULT;

		/* Only support setting timezone to 0 */
		if ((_tz.tz_minuteswest != 0) || (_tz.tz_dsttime != 0))
			return -EFAULT;
	}

	return 0;
}

