#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/percpu.h>
#include <lwk/aspace.h>
#include <lwk/sched.h>

struct run_queue {
	spinlock_t           lock;
	size_t               num_tasks;
	struct list_head     task_list;
	struct task_struct * idle_task;
};

static DEFINE_PER_CPU(struct run_queue, run_queue);

int
sched_init(void)
{
	id_t cpu_id;
	struct run_queue *runq;
	struct task_struct *idle_task;
	start_state_t start_state;
	int status;

	/* Initialize each CPU's run queue */
	for_each_cpu_mask(cpu_id, cpu_present_map) {
		runq = &per_cpu(run_queue, cpu_id);

		spin_lock_init(&runq->lock);
		runq->num_tasks = 0;
		list_head_init(&runq->task_list);

		/*
 	 	 * Create this CPU's idle task. When a CPU has no
 	 	 * other work to do, it runs the idle task. 
 	 	 */
		start_state.uid         = 0;
		start_state.gid         = 0;
		start_state.aspace_id   = KERNEL_ASPACE_ID;
		start_state.entry_point = (vaddr_t)arch_idle_task_loop;
		start_state.stack_ptr   = 0; /* will be set automatically */
		start_state.cpu_id      = cpu_id;
		start_state.cpumask     = NULL;

		status = __task_create(IDLE_TASK_ID, "idle_task", &start_state,
		                       &idle_task);
		if (status)
			panic("Failed to create idle_task (status=%d).",status);

		runq->idle_task = idle_task;
	}

	return 0;
}

void
sched_add_task(struct task_struct *task)
{
	struct run_queue *runq;
	unsigned long irqstate;

	runq = &per_cpu(run_queue, task->cpu_id);
	spin_lock_irqsave(&runq->lock, irqstate);
	list_add_tail(&task->sched_link, &runq->task_list);
	++runq->num_tasks;
	spin_unlock_irqrestore(&runq->lock, irqstate);
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

static void
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
}

void
schedule(void)
{
	struct run_queue *runq = &per_cpu(run_queue, cpu_id());
	struct task_struct *prev = current, *next = NULL, *task;

	BUG_ON(!irqs_disabled());

	spin_lock(&runq->lock);

	/* Move the currently running task to the end of the run queue */
	if (!list_empty(&prev->sched_link)) {
		list_del(&prev->sched_link);
		list_add_tail(&prev->sched_link, &runq->task_list);
	}

	/* Look for a ready to execute task */
	list_for_each_entry(task, &runq->task_list, sched_link) {
		if (task->state == TASKSTATE_READY) {
			next = task;
			break;
		}
	}

	/* If no tasks are ready to run, run the idle task */
	if (next == NULL)
		next = runq->idle_task;

	if (prev != next) {
		context_switch(prev, next);
		/* next is now running, since it may have changed CPUs while
		 * it was sleeping, we need to refresh local variables. */
		runq = &per_cpu(run_queue, cpu_id());
	}

	spin_unlock(&runq->lock);
}

void
schedule_new_task_tail(void)
{
	struct run_queue *runq = &per_cpu(run_queue, cpu_id());
	spin_unlock(&runq->lock);
}
