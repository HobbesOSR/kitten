/* Copyright (c) 2007-2010 Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <lwk/xcall.h>


// Caller must have aspace->lock locked
static bool
task_id_exists(struct aspace *aspace, id_t task_id)
{
	struct task_struct *tsk;

	list_for_each_entry(tsk, &aspace->task_list, aspace_link) {
		if (tsk->id == task_id)
			return true;
	}

	return false;
}


// Caller must have aspace->lock locked
static id_t
alloc_task_id(struct aspace *aspace, id_t want)
{
	id_t id, start;

	if (want == ANY_ID) {
		start = aspace->next_task_id;
		do {
			id = aspace->next_task_id;
			++aspace->next_task_id;
		} while (task_id_exists(aspace, id) && (aspace->next_task_id != start));

		// Handle case where all ID's have already been allocated
		if (aspace->next_task_id == start)
			id = ERROR_ID;
	} else {
		id = want;

		// Make sure ID isn't already allocated.
		// The IDLE_TASK_ID is special... there is one idle task per
		// CPU, each having the same ID.
		if (task_id_exists(aspace, id) && 
		    !((aspace->id == KERNEL_ASPACE_ID) && (want == IDLE_TASK_ID)))
			id = ERROR_ID;
	}

	return id;
}


// Caller must have aspace->lock locked
static id_t
alloc_cpu_id(struct aspace *aspace, id_t want)
{
	id_t id;

	if (want == ANY_ID) {
		id = aspace->next_cpu_id;

		// Increment next_cpu_id, limiting to values in aspace->cpu_mask
		aspace->next_cpu_id = next_cpu(aspace->next_cpu_id, aspace->cpu_mask);
		if (aspace->next_cpu_id == NR_CPUS)
			aspace->next_cpu_id = first_cpu(aspace->cpu_mask);
	} else {
		id = want;
		if (!cpu_isset(id, aspace->cpu_mask))
			return ERROR_ID;
	}

	return id;
}


struct task_struct *
__task_create(
	const start_state_t *	start_state,
	const struct pt_regs *	parent_regs
)
{
	struct aspace *aspace;
	union task_union *tsk_union;
	struct task_struct *tsk;
	int irqstate;

	// Lookup the target address space
	aspace = aspace_acquire(start_state->aspace_id);
	if (!aspace)
		return NULL;

	// Begin critical section
	spin_lock_irqsave(&aspace->lock, irqstate);

	// Stop right now if the process is in the process of dying
	if (aspace->exiting)
		goto fail_exiting;

	// Allocate a new task structure
	tsk_union = kmem_get_pages(TASK_ORDER);
	if (!tsk_union)
		goto fail_task_alloc;
	tsk = &tsk_union->task_info;

	// Figure out the new task's ID
	tsk->id = alloc_task_id(aspace, start_state->task_id);
	if (tsk->id == ERROR_ID)
		goto fail_task_id_alloc;

	// Figure out the new task's CPU ID
	tsk->cpu_id = alloc_cpu_id(aspace, start_state->cpu_id);
	if (tsk->cpu_id == ERROR_ID)
		goto fail_cpu_id_alloc;

	// Fill in and initialize the rest of the task structure
	tsk->state	=	TASK_RUNNING;
	tsk->uid	=	start_state->user_id;
	tsk->gid	=	start_state->group_id;
	tsk->aspace	=	aspace;
	tsk->cpu_mask	=	aspace->cpu_mask;
	strlcpy(tsk->name, start_state->task_name, sizeof(tsk->name));
	list_head_init(&tsk->aspace_link);
	list_head_init(&tsk->sched_link);
	list_head_init(&tsk->sigpending.list);
	
	// Do architecture-specific initialization
	if (arch_task_create(tsk, start_state, parent_regs))
		goto fail_arch;

	// Add the new task to the aspace's list of tasks
	list_add(&tsk->aspace_link, &aspace->task_list);

	// TODO: fix this stuff, it is broken
	{
		// Always "clone" files
		memcpy(tsk->files, current->files, sizeof(tsk->files));

		// Setup aliases needed for Linux compatibility layer
		tsk->comm = tsk->name;
		tsk->mm	  = tsk->aspace;
	}

	// End critical section
	spin_unlock_irqrestore(&aspace->lock, irqstate);

	// Success
	return tsk;

fail_arch:
fail_cpu_id_alloc:
fail_task_id_alloc:
	kmem_free_pages(tsk_union, TASK_ORDER);
fail_task_alloc:
fail_exiting:
	spin_unlock_irqrestore(&aspace->lock, irqstate);
	aspace_release(aspace);
	return NULL;
}


int
task_create(
	const start_state_t *	start_state,
	id_t *			task_id
)
{
	struct task_struct *tsk;

	// Create and initialize new task based on start_state
	tsk = __task_create(start_state, NULL);
	if (!tsk)
		return -EINVAL;

	// Add the new task to the target CPU's run queue
	sched_add_task(tsk);

	if (task_id)
		*task_id = tsk->id;

	return 0;

}


// Task termination handling.
//
// This function cleans up the state associated with the
// currently running task and frees the memory it was using.
// This function does not return to the caller.
void
task_exit(
	int	exit_status
)
{
	unsigned long irqstate;
	bool is_last_task = false;
	struct siginfo siginfo;

	// Begin critical section
	spin_lock_irqsave(&current->aspace->lock, irqstate);

	// Unbind task from its address space
	list_del_init(&current->aspace_link);

	// If this was the only task in the address space,
	// set the address space's exit_status to the exit_status
	// passed in.
	if ((current->aspace->exiting == false)
	    && list_empty(&current->aspace->task_list)
	    && (current->aspace->id != KERNEL_ASPACE_ID))
	{
		current->aspace->exiting     = true;
		current->aspace->exit_status = exit_status;
	}

	// If this is the last task in the address space,
	// record the information needed for the parent notification.
	if (list_empty(&current->aspace->task_list)
	    && (current->aspace->id != KERNEL_ASPACE_ID))
	{
		is_last_task = true;

		// Build the siginfo structure that will be delivered
		// to the parent aspace below
		siginfo.si_signo = SIGCHLD;
		siginfo.si_errno = 0;
		siginfo.si_pid   = current->aspace->id;
		siginfo.si_uid   = current->uid;

		if (current->aspace->exit_status & 0x7f) {
			siginfo.si_code   = CLD_KILLED;
			siginfo.si_status = current->aspace->exit_status & 0x7f;
		} else {
			siginfo.si_code   = CLD_EXITED;
			siginfo.si_status = current->aspace->exit_status >> 8;
		}
	}

	// End critical section
	spin_unlock_irqrestore(&current->aspace->lock, irqstate);
	
	// If this was the last task in its address space,
	// we need to notify the parent address space of this fact.
	// This is accomplished by sending the parent a SIGCHLD signal
	// and also waking up any of the parent's tasks that are waiting
	// for a child exit event.
	if (is_last_task) {
		sigsend(current->aspace->parent->id, ANY_ID, SIGCHLD, &siginfo);
		waitq_wakeup(&current->aspace->parent->child_exit_waitq);
	}

	// Decrement the aspace's reference count
	aspace_release(current->aspace);

	// schedule() will see that task has been marked TASK_EXITED
	// and will unlink the task from its run queue and free the
	// task structure. Therefore, this call never returns.
	set_task_state(current, TASK_EXITED);
	schedule();
	BUG();
}


void
task_exit_group(
	int	exit_status
)
{
	struct task_struct *tsk;
	unsigned long irqstate;

	// Begin critical section
	spin_lock_irqsave(&current->aspace->lock, irqstate);

	// If this is the first task in the address space to
	// be terminated, record why it terminated and then
	// send a SIGKILL to every other task in the aspace.
	if (current->aspace->exiting == false) {
		current->aspace->exiting     = true;
		current->aspace->exit_status = exit_status;

		list_for_each_entry(tsk, &current->aspace->task_list, aspace_link) {
			if (tsk != current) {
				sigset_add(&tsk->sigpending.sigset, SIGKILL);
				set_bit(TF_SIG_PENDING_BIT, &tsk->arch.flags);
				if (tsk->state == TASK_INTERRUPTIBLE) {
					set_task_state(tsk, TASK_RUNNING);
					xcall_reschedule(tsk->cpu_id);
				}
			}
		}
	}

	// End critical section
	spin_unlock_irqrestore(&current->aspace->lock, irqstate);

	// Terminate the calling task, this call does not return
	task_exit(exit_status);
}
