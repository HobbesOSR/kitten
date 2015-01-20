/** \file
 * One shot and periodic timers.
 *
 */
#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/smp.h>
#include <lwk/percpu.h>
#include <lwk/time.h>
#include <lwk/timer.h>
#include <lwk/sched.h>
#include <lwk/xcall.h>

struct timer_queue {
	spinlock_t       lock;
	struct list_head timer_list;
};

static DEFINE_PER_CPU(struct timer_queue, timer_queue);

/* Don't ask for timers shorter than 10 microseconds */
#define MIN_TIMER_INTERVAL 10000 

static void
interrupt_timer_init(void) {
/* Oneshot timer is started by the scheduler. */
#if defined(CONFIG_TIMER_PERIODIC)
        arch_set_timer_freq(sched_hz);
#endif
}

int
core_timer_init(int cpu_id)
{
	struct timer_queue *timerq = &per_cpu(timer_queue, cpu_id);
    
	spin_lock_init(&timerq->lock);
	list_head_init(&timerq->timer_list);
	interrupt_timer_init();

	return 0;
}



/** Set the timer interrupt to fire for the current head of
 *  the per-CPU timer list.
 */
static void 
set_timer_interrupt(void)
{
#ifdef CONFIG_TIMER_ONESHOT
	struct timer_queue *timerq = &per_cpu(timer_queue, this_cpu);
	if (!list_empty(&timerq->timer_list) ) {
		const uint64_t now = get_time();
		uint64_t diff;
		struct timer *timer = 
			list_entry( timerq->timer_list.next,
				    struct timer, link );

		if (timer->expires > (now + MIN_TIMER_INTERVAL)) {
			diff = timer->expires - now;
		} else {
			diff = MIN_TIMER_INTERVAL;
		}

		arch_set_timer_oneshot(diff);
	} 
#endif 
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

	set_timer_interrupt();

	spin_unlock_irqrestore(&timerq->lock, irqstate);
}


void 
timer_add_on( struct timer *timer, int cpu )
{
    printk("%s() XXXXXXXXXX\n",__func__);
    timer->cpu = cpu;

    cpumask_t   cpu_mask;
    cpus_clear( cpu_mask );
    cpu_set( cpu, cpu_mask );

    xcall_function( cpu_mask, (void*) timer_add, timer, 1 );
}


int
timer_del(struct timer *timer)
{
	unsigned long irqstate;
	int not_expired = 0;

	struct timer_queue *timerq = &per_cpu(timer_queue, timer->cpu);
	spin_lock_irqsave(&timerq->lock, irqstate);

	/* Remove the timer, if it hasn't already expired */
	if (!list_empty(&timer->link)) {
		list_del(&timer->link);
		not_expired = 1;
		set_timer_interrupt();
	}

	spin_unlock_irqrestore(&timerq->lock, irqstate);

	return not_expired;
}


/** Wakeup a sleeping task.
 *
 * Helper to wake a task that has gone to sleep until its timer expires.
 */
static void
wakeup_task(uintptr_t task)
{
	sched_wakeup_task((struct task_struct *)task, TASK_NORMAL);
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

	/* Set task to TASK_INTERRUPTIBLE state so it goes
	 * to sleep and other task can be scheduled
	 */

	set_task_state(current, TASK_INTERRUPTIBLE);


	/* Go to sleep */
	schedule();

	timer_del(&timer);

	/* Return the time remaining, if any */
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
		struct timer *timer = 
			list_entry( timerq->timer_list.next,
			struct timer, link
		);

		if( timer->expires > now )
			break;

		list_del_init(&timer->link);
		spin_unlock_irqrestore(&timerq->lock, irqstate);

		/* Execute the timer's callback function.
		 * Note that we have released the timerq->lock, so the
		 * callback function is free to call timer_add(). */
		if (timer->function)
			timer->function( timer->data );

		/* Get the lock again on the list */
		spin_lock_irqsave(&timerq->lock, irqstate);
	}

	set_timer_interrupt();

	spin_unlock_irqrestore(&timerq->lock, irqstate);
}
