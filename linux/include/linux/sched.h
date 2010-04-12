#ifndef _LINUX_SCHED_H
#define _LINUX_SCHED_H

#define pid id
#define tgid gid
#define cpus_allowed cpumask


#include <lwk/sched.h>
#include <linux/capability.h>
#include <asm/processor.h>
#define wake_up_process(p)      sched_wakeup_task((p), TASK_ALL)

/**
 * Linux trys to call schedule() during long latency operations.
 * For now LWK doesn't since there will usually be only one task per CPU.
 * We may need to reevaluate this later.
 */
static inline int
cond_resched(void)
{
	return 0;
}

/* Grab a reference to a task's mm, if it is not already going away */
#define mm_struct aspace
extern struct mm_struct *get_task_mm(struct task_struct *task);

#endif
