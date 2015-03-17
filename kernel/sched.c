/** \file
 * Process scheduler implementation.
 */
#include <lwk/smp.h>
#include <lwk/aspace.h>
#include <lwk/sched.h>
#include <lwk/timer.h>
#include <lwk/params.h>
#include <lwk/preempt_notifier.h>
#include <lwk/xcall.h>
#include <lwk/bootstrap.h>

#include <lwk/sched_rr.h>

#ifdef CONFIG_SCHED_EDF
#include <lwk/sched_edf.h>
#endif

/**
 * User-level tasks are preemptively scheduled sched_hz times per second.
 * Kernel threads are not preemptively scheduled and must call schedule()
 * themselves.
 */
unsigned int sched_hz = 10;  /* default to 10 Hz */
param(sched_hz, uint);

/**
 * Process run queue.
 *
 * Kitten maintains a round roubin and (optionally) an EDF runqueue.
 * A task is on only one queue; tasks with a non-zero EDF period are
 * scheduled VIA EDF, other tasks are scheduled via round robin.
 * EDF tasks have absolute priority over round robin tasks.
 * Since the number of tasks is expected to be low, the overhead in
 * either case is very small.
 *
 * If there are no tasks in the EDF or round robing scheduler, the idle task
 * is run instead.
 */
struct run_queue {
        spinlock_t           lock;
        size_t               num_tasks;
        int                  online;
        struct list_head     migrate_list;
        struct task_struct * idle_task;
	struct timer	     next_int;
        struct rr_rq rr;
#ifdef CONFIG_SCHED_EDF
        struct edf_rq edf;
#endif
};

static DEFINE_PER_CPU(struct run_queue, run_queue);

/** Spin until something else is ready to run */
void
idle_task_loop(void)
{
	struct run_queue *runq = &per_cpu(run_queue, this_cpu);

        while (1) {
                if (runq->online == 0) {
                        local_irq_disable();
                        kmem_free_pages(runq->idle_task, TASK_ORDER);
                        cpu_clear(this_cpu, cpu_online_map);
                        arch_idle_task_loop_body(0);

                        panic("CPU offline should not return!\n");
                } else {
			local_irq_disable();
			if (!test_bit(TF_NEED_RESCHED_BIT, &current->arch.flags)) {
                        	arch_idle_task_loop_body(1);
			} else {
				local_irq_enable();
			}
			schedule();
		}
        }
}

static void 
interrupt_task(uintptr_t task)
{
	return;
}

int __init
sched_init_runqueue(int cpu_id)
{
	struct run_queue *runq = &per_cpu(run_queue, cpu_id);

	spin_lock_init(&runq->lock);

	runq->num_tasks = 0;
	runq->online    = 1;
	list_head_init(&runq->migrate_list);
        runq->next_int.expires = 0;
        runq->next_int.function = interrupt_task;
        runq->next_int.data = 0;
        list_head_init(&runq->next_int.link);

	rr_sched_init_runqueue(&runq->rr, cpu_id);
#ifdef CONFIG_SCHED_EDF
	edf_sched_init_runqueue(&runq->edf, cpu_id);
#endif

        /*
         * Create this CPU's idle task. When a CPU has no
         * other work to do, it runs the idle task.
         */
        start_state_t start_state = {
                .task_id     = IDLE_TASK_ID,
                .task_name   = "idle_task",
                .user_id     = 0,
                .group_id    = 0,
                .aspace_id   = KERNEL_ASPACE_ID,
                .cpu_id      = cpu_id,
                .stack_ptr   = 0, /* will be set automatically */
                .entry_point = (vaddr_t) idle_task_loop,
                .use_args    = 0,
#ifdef CONFIG_SCHED_EDF
		.edf.period   = 0,
#endif
        };

        struct task_struct * idle_task = __task_create(&start_state, NULL);
        if (!idle_task)
                panic("Failed to create idle_task for CPU %u.", cpu_id);

        runq->idle_task = idle_task;

	return 0;
}

void sched_cpu_remove(void * arg)
{
	struct run_queue *runq = &per_cpu(run_queue, current->cpu_id);

	local_irq_disable();
	spin_lock(&runq->lock);

        runq->online = 0;

	rr_sched_cpu_remove(&runq->rr, arg);

#ifdef CONFIG_SCHED_EDF
	edf_sched_cpu_remove(&runq->edf, arg);
#endif
	spin_unlock(&runq->lock);
        local_irq_enable();
}
/* preempt_notifier functions (common to all schedulers)*/
void 
preempt_notifier_register(struct preempt_notifier * notifier)
{
	hlist_add_head(&notifier->link, &current->preempt_notifiers);
}


void 
preempt_notifier_unregister(struct preempt_notifier * notifier)
{
	hlist_del(&notifier->link);
}

void
fire_sched_out_preempt_notifiers(struct task_struct * curr,
				 struct task_struct * next)
{
	struct preempt_notifier * notifier;
	struct hlist_node * pos;

	hlist_for_each_entry(notifier, pos, &curr->preempt_notifiers, link) {
		notifier->ops->sched_out(notifier, next);
	}
}

void
fire_sched_in_preempt_notifiers(struct task_struct * curr) 
{
	struct preempt_notifier * notifier;
	struct hlist_node * pos;

	hlist_for_each_entry(notifier, pos, &curr->preempt_notifiers, link) {
		notifier->ops->sched_in(notifier, smp_processor_id());
	}
}

void
sched_add_task(struct task_struct *task)
{
	id_t cpu = task->cpu_id;
	struct run_queue *runq;
	unsigned long irqstate;

	runq = &per_cpu(run_queue, cpu);
	spin_lock_irqsave(&runq->lock, irqstate);
	++runq->num_tasks;

#ifdef CONFIG_SCHED_EDF
	if (task->edf.period) {
		edf_sched_add_task(&runq->edf, task);
	} else
#endif
	{
		rr_sched_add_task(&runq->rr, task);
	}
	spin_unlock_irqrestore(&runq->lock, irqstate);

	if (cpu != this_cpu)
		xcall_reschedule(cpu);
}

void
sched_del_task(struct task_struct *task)
{
	struct run_queue *runq;
	unsigned long irqstate;
	runq = &per_cpu(run_queue, task->cpu_id);
	spin_lock_irqsave(&runq->lock, irqstate);
	--runq->num_tasks;

#ifdef CONFIG_SCHED_EDF
	if (task->edf.period) {
		edf_sched_del_task(&runq->edf, task);
	} else
#endif
	{
		rr_sched_del_task(&runq->rr, task);
	}

	spin_unlock_irqrestore(&runq->lock, irqstate);

}



int
sched_wakeup_task(struct task_struct *task, taskstate_t valid_states)
{
	id_t cpu;
	struct run_queue *runq;
	int status;
	unsigned long irqstate;

	/* JRL: HACK!!
	 * This overloads the TASK_STOPPED state to effectively mean "uninitialized"
	 * This is OK for the moment, because there aren't any other users of TASK_STOPPED, 
	 * except GDB which doesn't even work currently. This could break easily in the future. 
	 * It would be preferable if we could just move a task to a scheduler whenever its parameters are set, 
	 * and default all new threads to RR. 
	 */
	if (task->state == TASK_STOPPED) {
		sched_add_task(task);
	}

	/* Protect against the task being migrated to a different CPU */
repeat_lock_runq:
	cpu  = task->cpu_id;
	runq = &per_cpu(run_queue, cpu);
	spin_lock_irqsave(&runq->lock, irqstate);
	if (cpu != task->cpu_id) {
		spin_unlock_irqrestore(&runq->lock, irqstate);
		goto repeat_lock_runq;
	}
	if (task->state & valid_states) {
		set_task_state(task, TASK_RUNNING);
		status = 0;
	} else {
		if (task->state == TASK_STOPPED)
			task->ptrace = (TASK_RUNNING << 1) | 1;
		status = -EINVAL;
	}
	spin_unlock_irqrestore(&runq->lock, irqstate);
#ifdef CONFIG_SCHED_EDF
	edf_adjust_reservation(&runq->edf, task);
#endif
	if (!status && (cpu != this_cpu))
		xcall_reschedule(cpu);

	return status;
}

/* Context switch function common to all schedulers*/
struct task_struct *
context_switch(struct task_struct *prev, struct task_struct *next)
{
#ifdef CONFIG_TASK_MEAS
	arch_task_meas();
#endif

	/* Switch to the next task's address space */
	if (prev->aspace != next->aspace)
		arch_aspace_activate(next->aspace);

	/**
	 * Switch to the next task's register state and kernel stack.
	 * There are three tasks involved in a context switch:
	 *     1. The previous task
	 *     2. The next task 
	 *     3. The task that was running when next was suspended
	 * arch_context_switch() returns 1 so that we can maintain
	 * the correct value of prev.  Otherwise, the restored register
	 * state of next would have prev set to 3, which we don't care
	 * about (it may have moved CPUs, been destroyed, etc.).
	 */
	prev = arch_context_switch(prev, next);

#ifdef CONFIG_TASK_MEAS
	arch_task_meas_start();
#endif

	/* Prevent compiler from optimizing beyond this point */
	barrier();

	return prev;
}

static void
set_quantum_timer(struct run_queue *runq, ktime_t inttime)
{
	timer_del(&runq->next_int);
	if (!inttime) {
        	const ktime_t now = get_time();
		inttime =  now + 1000000000ul/sched_hz;
	}
        runq->next_int.expires = inttime;
        timer_add(&runq->next_int);
	
	return;
}

void
schedule(void)
{
	struct run_queue *runq = &per_cpu(run_queue, this_cpu);
	struct task_struct *prev = current, *next = NULL, *task, *tmp;
	ktime_t inttime = 0;

	/* Remember prev's external interrupt state */
	prev->sched_irqs_on = irqs_enabled();

	local_irq_disable();
	spin_lock(&runq->lock);

	/* Fix race where signal arrived before caller set TASK_INTERRUPTIBLE */
	if ((prev->state == TASK_INTERRUPTIBLE) && signal_pending(prev))
		set_task_state(prev, TASK_RUNNING);

	/* Remove the currently running task from either the round-robin
	 * runqueue or migrate list to the end of the run queue. */

	if (!list_empty(&prev->migrate_link)) {
		list_del(&prev->migrate_link);
	}
	if(prev !=runq->idle_task){
		if (!list_empty(&prev->rr.sched_link)) {
			list_del(&prev->rr.sched_link);
		}
		if (prev->state != TASK_EXITED) {
			/* If the task is migrating, move it to the
			 * migrate_list.  The task will be migrated in
			 * the second-half of schedule().  Otherwise,
			 * link it back onto the task_list. */
			if (prev->cpu_id != prev->cpu_target_id) {
				list_add_tail(&prev->migrate_link,
					      &runq->migrate_list);
				--runq->num_tasks;

			} else {
#ifdef CONFIG_SCHED_EDF
				/*If not scheduled by EDF adjust rr schedule*/
				if (!prev->edf.period
				    ||	!edf_adjust_schedule(&runq->edf, prev))
#endif
				{
					rr_adjust_schedule(&runq->rr, prev);
				}
			}
		}
	}
	next = NULL;
	inttime = 0;
#ifdef CONFIG_SCHED_EDF
	next = edf_schedule(&runq->edf, &runq->migrate_list, &inttime);
#endif
	if (next == NULL) {
		next = rr_schedule(&runq->rr, &runq->migrate_list);
	}

	/* If no tasks are ready to run, run the idle task */
	if (next == NULL) {
		next = runq->idle_task;
	}

        /*Just in case a tight inttime has been set*/
        const ktime_t now = get_time();
        if(inttime && inttime < now){
                inttime = now + 1000000ul;
        }

	set_quantum_timer(runq, inttime);

#ifdef CONFIG_SCHED_EDF
	set_wakeup_task(&runq->edf,next);
#endif

	/* A reschedule has occurred, so clear prev's TF_NEED_RESCHED_BIT */
	clear_bit(TF_NEED_RESCHED_BIT, &prev->arch.flags);

	if (prev != next) {
		fire_sched_out_preempt_notifiers(prev, next);
		prev = context_switch(prev, next);

		/* "next" is now running. Since it may have changed CPUs
		 * while it was sleeping, we need to refresh local variables.
		 * Also note that the 'next' variable is stale... it is
		 * the value of next when the currently executing task
		 * originally called schedule() (i.e., when it was prev).
		 * 'current' should be used to refer to the currently
		 * executing task. */
		runq = &per_cpu(run_queue, this_cpu);

		fire_sched_in_preempt_notifiers(current);

		/* Free memory used by prev if it has exited */
		if (prev->state == TASK_EXITED) {
#ifdef CONFIG_SCHED_EDF
			/* Remove the current task from the EDF tree*/
		if(prev->edf.period){
			edf_sched_del_task(&runq->edf,prev);
		}
#endif
			kmem_free_pages(prev, TASK_ORDER);
		}
	}

	spin_unlock(&runq->lock);
	BUG_ON(irqs_enabled());

	/* Migrate any tasks that need migrating.
	 * Since schedule() is the only code that adds to the local CPUs
	 * migrate_list, and since interrupts are disabled, we can safely
	 * manipulate migrate_list without holding runq->lock. This is
	 * important/required for avoiding deadlock with other CPUs. */
	list_for_each_entry_safe(task, tmp, &runq->migrate_list, migrate_link) {
		id_t dest_cpu = task->cpu_target_id;
		struct run_queue *dest_runq = &per_cpu(run_queue, dest_cpu);

		/* Remove the task from the source runq's migrate_list */
		list_del(&task->migrate_link);
#ifdef CONFIG_SCHED_EDF
		/* Remove the current task from the EDF tree*/
	if(prev->edf.period){
		edf_sched_del_task(&runq->edf,prev);
	}
#endif
		/* Add the task to the destination runq's task_list */
		spin_lock(&dest_runq->lock);
		task->cpu_id = dest_cpu;
		/* XXX this should be a general schedule call for the
		 * task on that CPU, but not this one which saves IRQs! */
#ifdef CONFIG_SCHED_EDF
		if (task->edf.period) {
			edf_sched_add_task(&dest_runq->edf, task);
		} else
#endif
		{
			rr_sched_add_task(&dest_runq->rr, task);
		}

		list_head_init(&task->migrate_link);
		++dest_runq->num_tasks;
		spin_unlock(&dest_runq->lock);

		/* Tell the destination CPU to reschedule */
		if (dest_cpu != this_cpu)
			xcall_reschedule(dest_cpu);
	}

	/* Restore the scheduled task's external interrupt state
	 * to what what it was when it originally called schedule() */
	if (current->sched_irqs_on)
		local_irq_enable();
}

/* schedule_timeout function common to all schedulers*/
ktime_t
schedule_timeout(ktime_t timeout)
{
	/* Special case, sleep forever */
	if (timeout == MAX_SCHEDULE_TIMEOUT) {
		schedule();
		return timeout;
	}

	/* Figure out when to wake up, being careful about overflow */
	ktime_t now  = get_time();
	ktime_t when = now + timeout;
	if (when < now)
		when = TIME_T_MAX;

	return timer_sleep_until(when);
}

void
schedule_new_task_tail(struct task_struct *prev, struct task_struct *next)
{
        struct run_queue *runq = &per_cpu(run_queue, this_cpu);

        /* Free memory used by prev if it has exited.
         * Have to be careful not to free the bootstrap task_struct,
         * since it is not part of the kmem allocator heap. */
        if ((prev->state == TASK_EXITED) && (prev != &bootstrap_task)){
#ifdef CONFIG_SCHED_EDF
		/* Remove the current task from the EDF tree*/
		if(prev->edf.period){
			edf_sched_del_task(&runq->edf,prev);
		}
#endif
		kmem_free_pages(prev, TASK_ORDER);
	}

        spin_unlock(&runq->lock);
        BUG_ON(irqs_enabled());
        /* arch code will re-enable IRQs as part of starting the new task */
}

void
sched_yield(void)
{
#ifdef CONFIG_SCHED_EDF
	if(current->edf.period){
		edf_sched_yield();
	}
	else
#endif
	{
	rr_sched_yield();
	}
}

void
sched_yield_to(struct task_struct * task)
{
	/*If current task is trying to yield to a task in other core return*/
	if(current->cpu_id != task->cpu_id)
		return;

	struct run_queue *runq = &per_cpu(run_queue, this_cpu);

#ifdef CONFIG_SCHED_EDF
	if(current->edf.period){
		edf_sched_yield_to(&runq->edf, task);
	}
	else
#endif
	{
		rr_sched_yield_to(&runq->rr, task);
	}
}

void
sched_set_params(struct task_struct * task, ktime_t slice, ktime_t period)
{
	id_t cpu;
	struct run_queue *runq;
	unsigned long irqstate;

#ifdef CONFIG_SCHED_EDF
repeat_lock_runq:
	cpu  = task->cpu_id;
	runq = &per_cpu(run_queue, cpu);
	spin_lock_irqsave(&runq->lock, irqstate);
	if (cpu != task->cpu_id) {
		printk("Task %s, cpu %d, task_cpu %d\n", task->name, cpu, task->cpu_id);
		spin_unlock_irqrestore(&runq->lock, irqstate);
		goto repeat_lock_runq;
	}

	/*
	 * This task isn't in the runqueue yet,
	 * just set params and return
	 */

	if (task->state == TASK_STOPPED) {

		task->edf.slice             = slice;
		task->edf.period            = period;
		task->edf.release_slice     = slice;
		task->edf.release_period    = period;

		spin_unlock_irqrestore(&runq->lock, irqstate);

		return;
	}

	/*
	 * Task already in the runqueue
	 * If currently a RR task (not period), set the new params and add to the EDF runqueue
	 * If already an EDF Task, set the new params and reinsert in EDF runqueue
	 */


	if (task->edf.period) {
		edf_sched_del_task(&runq->edf, task);
	} else
	{
		rr_sched_del_task(&runq->rr, task);
		list_head_init(&task->rr.sched_link);
	}

	task->edf.slice             = slice;
	task->edf.period            = period;
	task->edf.release_slice     = slice;
	task->edf.release_period    = period;

	if (task->edf.period) {
		edf_sched_add_task(&runq->edf, task);
	} else
	{
		rr_sched_add_task(&runq->rr, task);
	}

	spin_unlock_irqrestore(&runq->lock, irqstate);
#endif
}
