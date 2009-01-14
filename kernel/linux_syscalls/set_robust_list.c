#include <lwk/kernel.h>

long
sys_set_robust_list(
	void __user *		head,
	size_t			len
)
{
	return -ENOSYS;
}
