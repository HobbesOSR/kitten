#include <linux/smp.h>

int smp_call_function(void(*func)(void *info), void *info, int wait)
{
	return on_each_cpu(func, info, wait);
}
