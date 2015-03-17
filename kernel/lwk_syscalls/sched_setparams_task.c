#include <lwk/sched_control.h>

void
sys_sched_setparams_task(
	int __user                    pid,
	int __user                    tid,
	ktime_t			      slice,
	ktime_t			      period
)
{
	sched_setparams_task(pid, tid, slice, period);
}
