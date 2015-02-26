/** \file
 * Process scheduler API (Round Robin Scheduler).
 *
 * Kitten maintains a linked list of tasks per CPU that are in a
 * ready to run state.  Since the number of tasks is expected to
 * be low, the overhead is very small.
 */
#ifndef _LWK_SCHED_RR_H
#define _LWK_SCHED_RR_H

#include <lwk/task.h>
#include <lwk/init.h>
#include <lwk/timer.h>

struct rr_rq {
	struct list_head taskq;
	struct timer nextint;
};

extern int __init rr_sched_init_runqueue(struct rr_rq *, int cpu_id);
extern void rr_sched_add_task(struct rr_rq *, struct task_struct *task);
extern void rr_sched_del_task(struct rr_rq *, struct task_struct *task);
extern int rr_sched_wakeup_task(struct task_struct *task,
                             taskstate_t valid_states);

extern void rr_sched_cpu_remove(struct rr_rq *, void *);
extern struct task_struct *rr_schedule(struct rr_rq *, struct list_head *);

extern ktime_t rr_schedule_timeout(ktime_t timeout);
extern void rr_adjust_schedule(struct rr_rq *, struct task_struct *task);
extern void rr_sched_yield(void);
extern void rr_sched_yield_to(struct rr_rq *, struct task_struct *);

#endif
