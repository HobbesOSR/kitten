#include <lwk/kernel.h>
#include <lwk/time.h>
#include <arch/uaccess.h>

/* from /usr/include/linux/time.h */
#define CLOCK_REALTIME                  0

int
sys_clock_gettime(
	clockid_t which_clock,
	struct timespec* tp
)
{
	uint64_t when = get_time();

        struct timespec _rem = {
                .tv_sec  = when / NSEC_PER_SEC,
                .tv_nsec = when % NSEC_PER_SEC,
        };
	if ( which_clock != CLOCK_REALTIME ) 
		return -EINVAL;	

        if (copy_to_user(tp, &_rem, sizeof(_rem)))
                return -EFAULT;

	return 0;
}
