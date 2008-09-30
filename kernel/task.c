#include <lwk/kernel.h>
#include <lwk/xcall.h>
#include <lwk/htable.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <lwk/sched.h>
#include <arch/uaccess.h>

/**
 * ID space used to allocate task IDs.
 */
static idspace_t idspace;

/**
 * Hash table used to lookup task structures by ID.
 */
static htable_t htable;

/**
 * Lock for serializing access to the htable.
 */
static DEFINE_SPINLOCK(htable_lock);

int
task_init(void)
{
	if (idspace_create(__TASK_MIN_ID, __TASK_MAX_ID, &idspace))
		panic("Failed to create task ID space.");

	if (htable_create(7 /* 2^7 bins */,
	                  offsetof(struct task_struct, id),
	                  offsetof(struct task_struct, ht_link),
	                  &htable))
		panic("Failed to create task hash table.");

	return 0;
}

int
task_get_myid(id_t *id)
{
	*id = current->id;
	return 0;
}

int
sys_task_get_myid(id_t __user *id)
{
	int status;
	id_t _id;

	if ((status = task_get_myid(&_id)) != 0)
		return status;

	if (id && copy_to_user(id, &_id, sizeof(*id)))
		return -EINVAL;

	return 0;
}

int
__task_reserve_id(id_t id)
{
	return idspace_alloc_id(idspace, id, NULL);
}

int
__task_create(id_t id, const char *name,
              const start_state_t *start_state,
              struct task_struct **task)
{
	int status;
	union task_union *task_union;
	struct task_struct *tsk;

	if ((task_union = kmem_get_pages(TASK_ORDER)) == NULL)
		return -ENOMEM;

	tsk = &task_union->task_info;

	/*
	 * Initialize the new task. kmem_alloc() allocates zeroed memory
	 * so fields with an initial state of zero do not need to be explicitly
	 * initialized.
	 */
	tsk->id = id;
	if (name)
		strlcpy(tsk->name, name, sizeof(tsk->name));
	hlist_node_init(&tsk->ht_link);
	tsk->state = TASKSTATE_READY;
	tsk->uid = start_state->uid;
	tsk->gid = start_state->gid;
	tsk->aspace = aspace_acquire(start_state->aspace_id);
	if (!tsk->aspace) {
		status = -ENOENT;
		goto error1;
	}
	tsk->sighand = NULL;
	if (start_state->cpumask) {
		cpumask_user2kernel(start_state->cpumask, &tsk->cpumask);
		if (!cpus_subset(tsk->cpumask, current->cpumask)) {
			status = -EINVAL;
			goto error2;
		}
	} else {
		tsk->cpumask = current->cpumask;
	}
	if ((start_state->cpu_id >= NR_CPUS)
	     || !cpu_isset(start_state->cpu_id, tsk->cpumask)) {
		status = -EINVAL;
		goto error2;
	}
	tsk->cpu_id = start_state->cpu_id;
	list_head_init(&tsk->sched_link);
	tsk->ptrace = 0;
	tsk->flags = 0;
	tsk->exit_status = 0;

	/* Do architecture-specific initialization */
	if ((status = arch_task_create(tsk, start_state)) != 0)
		goto error2;

	if (task)
		*task = tsk;
	return 0;

error2:
	if (tsk->aspace)
		aspace_release(tsk->aspace);
error1:
	kmem_free_pages(task_union, TASK_ORDER);
	return status;
}

int
task_create(id_t id_request, const char *name,
            const start_state_t *start_state, id_t *id)
{
	id_t new_id;
	struct task_struct *new_task;
	int status;
	unsigned long irqstate;

	/* Allocate an ID for the new task */
	if ((status = idspace_alloc_id(idspace, id_request, &new_id)) != 0)
		return status;

	/* Create and initialize a new task */
	if ((status = __task_create(new_id, name, start_state, &new_task))) {
		idspace_free_id(idspace, new_id);
		return status;
	}

	/* Add new task to a hash table, for quick lookups by ID */
	spin_lock_irqsave(&htable_lock, irqstate);
	BUG_ON(htable_add(htable, new_task));
	spin_unlock_irqrestore(&htable_lock, irqstate);

	/* Add the new task to the target CPU's run queue */
	sched_add_task(new_task);

	if (id)
		*id = new_task->id;
	return 0;
}

int
sys_task_create(id_t id_request, const char __user *name,
                const start_state_t __user *start_state, id_t __user *id)
{
	int status;
	start_state_t _start_state;
	user_cpumask_t _cpumask;
	char _name[16];
	id_t _id;

	if (current->uid != 0)
		return -EPERM;

	if (copy_from_user(&_start_state, start_state, sizeof(_start_state)))
		return -EINVAL;

	if (_start_state.aspace_id == KERNEL_ASPACE_ID)
		return -EINVAL;

	if (_start_state.cpumask) {
		if (copy_from_user(&_cpumask, _start_state.cpumask, sizeof(_cpumask)))
			return -EINVAL;
		_start_state.cpumask = &_cpumask;
	}

	if (name && (strncpy_from_user(_name, name, sizeof(_name)) < 0))
		return -EFAULT;
	_name[sizeof(_name) - 1] = '\0';

	if ((status = task_create(id_request, _name, &_start_state, &_id)) != 0)
		return status;

	if (id && copy_to_user(id, &_id, sizeof(*id)))
		return -EFAULT;

	return 0;
}

int
task_exit(int status)
{
	/* Mark the task as exited...
	 * schedule() will remove it from the run queue */
	current->exit_status = status;
	current->state = TASKSTATE_EXIT_ZOMBIE;
	schedule(); /* task is dead, so this should never return */
	BUG();
	while (1) {}
}

int
sys_task_exit(int status)
{
	return task_exit(status);
}

int
task_yield(void)
{
	schedule();
	return 0;
}

int
sys_task_yield(void)
{
	return task_yield();
}
