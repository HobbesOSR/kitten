#include <lwk/sched_control.h>

void
sys_sched_yield_task_to(
	int __user                    pid,
	int __user                    tid
)
{
	sched_yield_task_to(pid, tid);
}
