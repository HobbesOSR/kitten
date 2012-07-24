#include <lwk/kernel.h>
#include <lwk/rlimit.h>
#include <arch/uaccess.h>

long
sys_getrlimit(
	unsigned int			resource,
	struct rlimit __user *		rlim
)
{
	struct rlimit rlimit;

	switch (resource) {
		case RLIMIT_AS:
			rlimit.rlim_cur = RLIM_INFINITY;
			rlimit.rlim_max = RLIM_INFINITY;
			break;
		case RLIMIT_STACK:
			rlimit.rlim_cur = RLIM_INFINITY;
			rlimit.rlim_max = RLIM_INFINITY;
			break;
		case RLIMIT_MEMLOCK:
			rlimit.rlim_cur = RLIM_INFINITY;
			rlimit.rlim_max = RLIM_INFINITY;
			break;
		default:
			printk("Unsupported rlimit (resource=%d).\n", resource);
			return -EINVAL;
	}

	if (copy_to_user(rlim, &rlimit, sizeof(rlimit)))
		return -EFAULT;

	return 0;
}
