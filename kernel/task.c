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

#if 1
/**
 * Looks up a task structure by ID and returns it with its spinlock locked.
 */
struct task_struct *
lookup_and_lock(id_t id)
{
	struct task_struct *task;

	/* Lock the hash table, lookup task by ID */
	spin_lock(&htable_lock);
	if ((task = htable_lookup(htable, id)) == NULL) {
		spin_unlock(&htable_lock);
		return NULL;
	}

#if 0
	/* Lock the identified task */
	spin_lock(&task->lock);
#endif

	/* Unlock the hash table, others may now use it */
	spin_unlock(&htable_lock);

	return task;
}
#endif

#if 0
void
sched_xcall(void *info)
{
	printk("Hello from sched xcall!\n");
}
#endif

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
__task_create(id_t id_request, const char *name,
              const start_state_t *start_state,
              struct task_struct **new_task)
{
	int status;
	id_t new_id;
	union task_union *task_union;
	struct task_struct *task;

	if ((status = idspace_alloc_id(idspace, id_request, &new_id)) != 0) {
		if (id_request != IDLE_TASK_ID)
			return status;
	}
	
	if ((task_union = kmem_get_pages(TASK_ORDER)) == NULL) {
		idspace_free_id(idspace, new_id);
		return -ENOMEM;
	}

	task = &task_union->task_info;

	/*
	 * Initialize the new task. kmem_alloc() allocates zeroed memory
	 * so fields with an initial state of zero do not need to be explicitly
	 * initialized.
	 */
	task->id = new_id;
	if (name)
		strlcpy(task->name, name, sizeof(task->name));
	hlist_node_init(&task->ht_link);
	task->state = TASKSTATE_READY;
	task->uid = start_state->uid;
	task->gid = start_state->gid;
	task->aspace = aspace_acquire(start_state->aspace_id);
	if (!task->aspace) {
		status = -ENOENT;
		goto error1;
	}
	task->sighand = NULL;
	if (start_state->cpumask) {
		cpumask_user2kernel(start_state->cpumask, &task->cpumask);
		if (!cpus_subset(task->cpumask, current->cpumask)) {
			status = -EINVAL;
			goto error2;
		}
	} else {
		task->cpumask = current->cpumask;
	}
	if ((start_state->cpu_id >= NR_CPUS)
	     || !cpu_isset(start_state->cpu_id, task->cpumask)) {
		status = -EINVAL;
		goto error2;
	}
	task->cpu_id = start_state->cpu_id;
	list_head_init(&task->sched_link);
	task->ptrace = 0;
	task->flags = 0;

	/* Do architecture-specific initialization */
	if ((status = arch_task_create(task, start_state)) != 0)
		goto error2;

	if (new_task)
		*new_task = task;
	return 0;

error2:
	if (task->aspace)
		aspace_release(task->aspace);
error1:
	idspace_free_id(idspace, task->id);
	kmem_free_pages(task_union, TASK_ORDER);
	return status;
}

int
task_create(id_t id_request, const char *name,
            const start_state_t *start_state, id_t *id)
{
	struct task_struct *task;
	int status;
	unsigned long flags;

	/* Create and initialize a new task */
	if ((status = __task_create(id_request, name, start_state, &task)))
		return status;

	/* Add new task to a hash table, for quick lookups by ID */
	spin_lock_irqsave(&htable_lock, flags);
	BUG_ON(htable_add(htable, task));
	spin_unlock_irqrestore(&htable_lock, flags);

	/* Add the new task to the target CPU's run queue */
	sched_add_task(task);

	*id = task->id;
	return 0;
}

extern int
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

	if (!start_state || (start_state->aspace_id == ANY_ID))
		return -EINVAL;

	if (copy_from_user(&_start_state, start_state, sizeof(_start_state)))
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

#if 0
int
task_run(id_t id)
{
#if 0
	int status = 0;
	struct task_struct *task;
	id_t cpu_id = 0;
	struct sched_info *sched;
	unsigned long irqstate;
	bool kick_target_cpu = false;

	/* First, figure out which CPU the task should be run on */
	local_irq_save(irqstate);
	if ((task = lookup_and_lock(id)) == NULL) {
		status = -ENOENT;
		goto out1;
	}
	cpu_id = task->cpu_id;
	spin_unlock(&task->lock);
	local_irq_restore(irqstate);

again:
	local_irq_save(irqstate);
	sched = &per_cpu(sched_info, cpu_id);
	spin_lock(&sched->lock);

	if ((task = lookup_and_lock(id)) == NULL) {
		status = -ENOENT;
		goto out2;
	}

	if (task->cpu_id != cpu_id) {
		spin_unlock(&task->lock);
		spin_unlock(&sched->lock);
		local_irq_restore(irqstate);
		goto again;
	}

	if (task->state == TASKSTATE_TERMINATED) {
		status = -EINVAL;
		goto out3;
	}

	if ((task->state == TASKSTATE_READY)
	     || (task->state == TASKSTATE_RUNNING)) { 
		goto out3;
	}

	task->state = TASKSTATE_READY;
	list_add_tail(&task->ready_list_link, &sched->ready_list);
	++sched->num_ready;
	kick_target_cpu = true;

out3:
	spin_unlock(&task->lock);
out2:
	spin_unlock(&sched->lock);
out1:
	local_irq_restore(irqstate);

	if (kick_target_cpu) {
		cpumask_t mask;
		cpus_clear(mask);
		cpu_set(cpu_id, mask);
		xcall_function(mask, sched_xcall, NULL, 1);
	}

	return status;
#endif
	return 0;
}

int
sys_task_run(id_t id)
{
	if (current->uid != 0)
		return -EPERM;
	return task_run(id);
}
#endif
