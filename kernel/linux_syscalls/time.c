#include <lwk/kernel.h>
#include <lwk/time.h>
#include <arch/uaccess.h>

time_t
sys_time(time_t __user *t)
{
	time_t now_sec = (time_t)(get_time() / NSEC_PER_SEC);

	if (t && copy_to_user(t, &now_sec, sizeof(now_sec)))
		return -EFAULT;

	return now_sec;
}
