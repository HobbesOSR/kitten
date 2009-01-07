/** \file
 * One shot and periodic timers.
 *
 */
#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/percpu.h>
#include <lwk/time.h>
#include <lwk/timer.h>
#include <lwk/sched.h>

struct timer_queue {
	spinlock_t       lock;
	struct list_head timer_list;
};

static DEFINE_PER_CPU(struct timer_queue, timer_queue);

int
timer_subsys_init(void)
{
	id_t cpu;
	struct timer_queue *timerq;

	for_each_cpu_mask(cpu, cpu_present_map) {
		timerq = &per_cpu(timer_queue, cpu);
		spin_lock_init(&timerq->lock);
		list_head_init(&timerq->timer_list);
	}

	return 0;
}


void
timer_add(struct timer *timer)
{
	struct list_head *pos;
	unsigned long irqstate;

	struct timer_queue *timerq = &per_cpu(timer_queue, this_cpu);
	spin_lock_irqsave(&timerq->lock, irqstate);

	/* Initialize fields we don't expect the caller to set */
	list_head_init(&timer->link);
	timer->cpu = this_cpu;

	/* Insert the new timer into the CPU's sorted timer_list */
	list_for_each(pos, &timerq->timer_list) {
		struct timer *cur = list_entry(pos, struct timer, link);
		if (cur->expires > timer->expires)
			break;
	}
	list_add_tail(&timer->link, pos);

	spin_unlock_irqrestore(&timerq->lock, irqstate);
}


void
timer_del(struct timer *timer)
{
	unsigned long irqstate;

	struct timer_queue *timerq = &per_cpu(timer_queue, timer->cpu);
	spin_lock_irqsave(&timerq->lock, irqstate);

	/* Remove the timer, if it hasn't already expired */
	if (!list_empty(&timer->link))
		list_del(&timer->link);

	spin_unlock_irqrestore(&timerq->lock, irqstate);
}


/** Wakeup a sleeping task.
 *
 * Helper to wake a task that has gone to sleep until its timer expires.
 */
static void
wakeup_task(uintptr_t task)
{
	sched_wakeup_task((struct task_struct *)task, TASKSTATE_INTERRUPTIBLE);
}


/* Returns the time remaining */
uint64_t
timer_sleep_until(uint64_t when)
{
	struct timer timer = {
		.expires	= when,
		.function	= &wakeup_task,
		.data     	= (uintptr_t) current,
	};

	timer_add(&timer);

	/* Go to sleep */
	set_mb(current->state, TASKSTATE_INTERRUPTIBLE);
	schedule();

	/* Return the time remaining */
	uint64_t now = get_time();
	return (when > now) ? (when - now) : 0;
}


/** Walk the per-CPU timer list, calling any timer callbacks.
 *
 * The struct timer entries will be unlinked, but not deleted.
 * It is up to the caller to free any dynamically allocated
 * structures.
 */
void
expire_timers(void)
{
	struct timer_queue *timerq = &per_cpu(timer_queue, this_cpu);
	const uint64_t now = get_time();
	unsigned long irqstate;

	spin_lock_irqsave(&timerq->lock, irqstate);

	while( !list_empty(&timerq->timer_list) )
	{
		struct timer *timer = list_entry(
			timerq->timer_list.next,
			struct timer,
			link
		);

		if( timer->expires > now )
			break;

		list_del_init(&timer->link);
		spin_unlock_irqrestore(&timerq->lock, irqstate);

		/* Execute the timer's callback function.
		 * Note that we have released the timerq->lock, so the
		 * callback function is free to call timer_add(). */
		timer->function( timer->data );

		/* Get the lock again on the list */
		spin_lock_irqsave(&timerq->lock, irqstate);
	}

	spin_unlock_irqrestore(&timerq->lock, irqstate);
}
