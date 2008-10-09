#include <lwk/kernel.h>
#include <lwk/task.h>

int
sys_sched_yield(void)
{
	return task_yield();
}
