#include <lwk/time.h>

int sys_clock_getres(clockid_t clk_id, struct timespec __user *tp)
{
	struct timespec tmp;

	tmp.tv_sec = 0;
	tmp.tv_nsec = 1000000000;
	printk("%s: set tp %p tv_sec %ld tv_nsec %ld\n", __func__, tp, tmp.tv_sec, tmp.tv_nsec);
	if(copy_to_user(tp, &tmp, sizeof(struct timespec))){
		return -EFAULT;
	}

	return 0;
	
}
