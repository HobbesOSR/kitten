/** \file
 * Process scheduler implementation.
 */
#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/smp.h>
#include <lwk/percpu.h>
#include <lwk/aspace.h>
#include <lwk/sched.h>
#include <lwk/xcall.h>
#include <lwk/timer.h>
#include <lwk/bootstrap.h>
#include <lwk/params.h>


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
 * Kitten maintains a linked list of tasks per CPU that are in a
 * ready to run state.  Since the number of tasks is expected to
 * be low, the overhead is very smal.
 *
 * If there are no tasks on the queue's task_list, the idle task
 * is run instead.
 */
struct run_queue {
	spinlock_t           lock;
	size_t               num_tasks;
	struct list_head     task_list;
	struct list_head     migrate_list;
	struct task_struct * idle_task;
};

static DEFINE_PER_CPU(struct run_queue, run_queue);


/** Spin until something else is ready to run */
static void
idle_task_loop(void)
{
	while (1) {
		arch_idle_task_loop_body();
		schedule();
	}
}


int __init
sched_subsys_init(void)
{
	id_t cpu_id;

	/* Initialize each CPU's run queue */
	for_each_cpu_mask(cpu_id, cpu_present_map) {
		struct run_queue *runq = &per_cpu(run_queue, cpu_id);

		spin_lock_init(&runq->lock);
		runq->num_tasks = 0;
		list_head_init(&runq->task_list);
		list_head_init(&runq->migrate_list);

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
		};

		struct task_struct *idle_task = __task_create(&start_state, NULL);
		if (!idle_task)
			panic("Failed to create idle_task for CPU %u.", cpu_id);

		runq->idle_task = idle_task;
	}

	return 0;
}

void
sched_add_task(struct task_struct *task)
{
	id_t cpu = task->cpu_id;
	struct run_queue *runq;
	unsigned long irqstate;

	runq = &per_cpu(run_queue, cpu);
	spin_lock_irqsave(&runq->lock, irqstate);
	list_add_tail(&task->sched_link, &runq->task_list);
	++runq->num_tasks;
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
	list_del(&task->sched_link);
	--runq->num_tasks;
	spin_unlock_irqrestore(&runq->lock, irqstate);
}

int
sched_wakeup_task(struct task_struct *task, taskstate_t valid_states)
{
	id_t cpu;
	struct run_queue *runq;
	int status;
	unsigned long irqstate;

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

	if (!status && (cpu != this_cpu))
		xcall_reschedule(cpu);

	return status;
}

static struct task_struct *
context_switch(struct task_struct *prev, struct task_struct *next)
{
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

	/* Prevent compiler from optimizing beyond this point */
	barrier();

	return prev;
}

void
schedule(void)
{
	struct run_queue *runq = &per_cpu(run_queue, this_cpu);
	struct task_struct *prev = current, *next = NULL, *task, *tmp;

	/* Remember prev's external interrupt state */
	prev->sched_irqs_on = irqs_enabled();

	local_irq_disable();
	spin_lock(&runq->lock);

	/* Fix race where signal arrived before caller set TASK_INTERRUPTIBLE */
	if ((prev->state == TASK_INTERRUPTIBLE) && signal_pending(prev))
		set_task_state(prev, TASK_RUNNING);

	/* Move the currently running task to the end of the run queue.
	 * This implements round-robin scheduling. */
	if (!list_empty(&prev->sched_link)) {
		list_del(&prev->sched_link);

		/* If the task has exited, don't re-link it to any run queue list.
		 * The task will be deleted in the second-half of schedule(). */
		if (prev->state != TASK_EXITED) {
			/* If the task is migrating, move it to the migrate_list.
			 * The task will be migrated in the second-half of schedule().
			 * Otherwise, link it back onto the task_list. */
			if (prev->cpu_id != prev->cpu_target_id) {
				list_add_tail(&prev->sched_link, &runq->migrate_list);
				--runq->num_tasks;
			} else {
				list_add_tail(&prev->sched_link, &runq->task_list);
			}
		}
	}

	/* Look for a ready to execute task */
	list_for_each_entry_safe(task, tmp, &runq->task_list, sched_link) {
		/* If the task is migrating, move it to the migrate_list.
		 * The task will be migrated in the second-half of schedule(). */
		if (task->cpu_id != task->cpu_target_id) {
			list_del(&task->sched_link);
			list_add_tail(&task->sched_link, &runq->migrate_list);
			continue;
		}

		/* Pick the first running task */
		if (task->state == TASK_RUNNING) {
			next = task;
			break;
		}
	}

	/* If no tasks are ready to run, run the idle task */
	if (next == NULL)
		next = runq->idle_task;

	/* A reschedule has occurred, so clear prev's TF_NEED_RESCHED_BIT */
	clear_bit(TF_NEED_RESCHED_BIT, &prev->arch.flags);

	if (prev != next) {
		prev = context_switch(prev, next);

		/* "next" is now running. Since it may have changed CPUs
		 * while it was sleeping, we need to refresh local variables.
		 * Also note that the 'next' variable is stale... it is
		 * the value of next when the currently executing task
		 * originally called schedule() (i.e., when it was prev).
		 * 'current' should be used to refer to the currently
		 * executing task. */
		runq = &per_cpu(run_queue, this_cpu);

		/* Free memory used by prev if it has exited */
		if (prev->state == TASK_EXITED)
			kmem_free_pages(prev, TASK_ORDER);
	}

	spin_unlock(&runq->lock);
	BUG_ON(irqs_enabled());

	/* Migrate any tasks that need migrating.
	 * Since schedule() is the only code that adds to the local CPUs 
	 * migrate_list, and since interrupts are disabled, we can safely
	 * manipulate migrate_list without holding runq->lock. This is
	 * important/required for avoiding deadlock with other CPUs. */
	list_for_each_entry_safe(task, tmp, &runq->migrate_list, sched_link) {
		id_t dest_cpu = task->cpu_target_id;
		struct run_queue *dest_runq = &per_cpu(run_queue, dest_cpu);

		/* Remove the task from the source runq's migrate_list */
		list_del(&task->sched_link);

		/* Add the task to the destination runq's task_list */
		spin_lock(&dest_runq->lock);
		task->cpu_id = dest_cpu;
		list_add_tail(&task->sched_link, &dest_runq->task_list);
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

void
schedule_new_task_tail(struct task_struct *prev, struct task_struct *next)
{
	struct run_queue *runq = &per_cpu(run_queue, this_cpu);

	/* Free memory used by prev if it has exited.
	 * Have to be careful not to free the bootstrap task_struct,
	 * since it is not part of the kmem allocator heap. */
	if ((prev->state == TASK_EXITED) && (prev != &bootstrap_task))
		kmem_free_pages(prev, TASK_ORDER);

	spin_unlock(&runq->lock);
	BUG_ON(irqs_enabled());
	/* arch code will re-enable IRQs as part of starting the new task */
}

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
