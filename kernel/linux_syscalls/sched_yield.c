#include <lwk/kernel.h>
#include <lwk/sched.h>

int
sys_sched_yield(void)
{
	schedule();
	return 0;
}
