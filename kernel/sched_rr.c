/** \file
 * Process scheduler implementation.
 */
#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/smp.h>
#include <lwk/percpu.h>
#include <lwk/aspace.h>
#include <lwk/sched.h>
#include <lwk/sched_rr.h>
#include <lwk/xcall.h>
#include <lwk/timer.h>
#include <lwk/bootstrap.h>
#include <lwk/params.h>
#include <lwk/preempt_notifier.h>
#include <lwk/kthread.h>
#include <lwk/task.h>

/**
 * User-level tasks are preemptively scheduled sched_hz times per second.
 * Kernel threads are not preemptively scheduled and must call schedule()
 * themselves.
 */

/**
 * Process run queue.
 *
 * Kitten maintains a linked list of tasks per CPU that are in a
 * ready to run state.  Since the number of tasks is expected to
 * be low, the overhead is very smal.
 *
 * If there are no tasks on the queue's taskq, the idle task
 * is run instead.
 */

int
rr_sched_init_runqueue(struct rr_rq *q, int cpu_id) {
	list_head_init(&q->taskq);
	return 0;
}

void rr_adjust_schedule(struct rr_rq *q, struct task_struct *task)
{
	list_add_tail(&task->rr.sched_link, &q->taskq);
}

/* Migrate all tasks away. Called with the runq lock held and local
 * IRQs disabled!
 */
void rr_sched_cpu_remove(struct rr_rq *runq, void * arg)
{
	struct task_struct *task, *tmp;

	list_for_each_entry_safe(task, tmp, &runq->taskq, rr.sched_link) {
                kthread_bind(task, 0);
                cpu_clear(this_cpu, task->aspace->cpu_mask);
	}
}

void
rr_sched_add_task(struct rr_rq *rr, struct task_struct *task)
{
	list_add_tail(&task->rr.sched_link, &rr->taskq);
}

void
rr_sched_del_task(struct rr_rq *runq, struct task_struct *task)
{
	list_del(&task->rr.sched_link);
}

struct task_struct *
rr_schedule(struct rr_rq *runq, struct list_head *migrate_list)
{
	struct task_struct *task, *tmp, *next = NULL;

	/* Look for a ready to execute task */
	list_for_each_entry_safe(task, tmp, &runq->taskq, rr.sched_link) {
		/* If the task is migrating, move it to the migrate_list.
		 * The task will be migrated in the second-half of
		 * schedule(). */

	        if (task->cpu_id != task->cpu_target_id) {
			list_del(&task->rr.sched_link);
			list_add_tail(&task->migrate_link, migrate_list);
			continue;
		}

		/* Pick the first running task */
		if (task->state == TASK_RUNNING) {
			next = task;
			break;
		}
	}

	return next;
}
