#include <lwk/kernel.h>
#include <lwk/xcall.h>
#include <lwk/htable.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <lwk/sched.h>
#include <lwk/smp.h>
#include <arch/uaccess.h>

/**
 * ID space used to allocate user-space task IDs.
 */
static struct idspace *user_idspace;

/**
 * ID space used to allocate kernel-space task IDs.
 */
static struct idspace *kernel_idspace;

/**
 * Hash table used to lookup task structures by ID.
 */
static struct htable *htable;

/**
 * Lock for serializing access to the hash table.
 */
static DEFINE_SPINLOCK(htable_lock);

int __init
task_subsys_init(void)
{
	user_idspace = idspace_create(
				TASK_MIN_ID,
				TASK_MAX_ID
	);
	if (!user_idspace)
		panic("Failed to create ID space for user tasks.");

	kernel_idspace = idspace_create(
				KERNEL_TASK_MIN_ID,
				KERNEL_TASK_MAX_ID
	);
	if (!kernel_idspace)
		panic("Failed to create ID space for kernel tasks.");

	htable = htable_create(
			7,  /* 2^7 bins in the hash table */
			offsetof(struct task_struct, id),
			offsetof(struct task_struct, ht_link),
			htable_id_hash,
			htable_id_key_compare
	);
	if (!htable)
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
		return -EFAULT;

	return 0;
}

static bool
is_user_id(id_t id)
{
	return (id >= TASK_MIN_ID) && (id <= TASK_MAX_ID);
}

static bool
is_kernel_id(id_t id)
{
	return (id >= KERNEL_TASK_MIN_ID) && (id <= KERNEL_TASK_MAX_ID);
}

static id_t
alloc_id(
	id_t			id_request,
	const start_state_t *	start_state
)
{
	bool is_kernel_task = (start_state->aspace_id == KERNEL_ASPACE_ID);
	id_t id;

	if (id_request != ANY_ID) {
		if (is_kernel_task && !is_kernel_id(id_request))
			return ERROR_ID;
		if (!is_kernel_task && !is_user_id(id_request))
			return ERROR_ID;
	}

	id = idspace_alloc_id(
			is_kernel_task ? kernel_idspace : user_idspace,
			id_request
	);

	/* The idle task is a special case...
 	 * There is one idle task per CPU, all with the same ID */
	if ((id == ERROR_ID) && (id_request == IDLE_TASK_ID) && is_kernel_task)
		id = IDLE_TASK_ID;

	return id;
}

static void
free_id(
	id_t			id
)
{
	idspace_free_id(
		is_kernel_id(id) ? kernel_idspace : user_idspace,
		id
	);
}

struct task_struct *
__task_create(
	id_t			id_request,
	const char *		name,
	const start_state_t *	start_state,
	const struct pt_regs *	parent_regs
)
{
	id_t id;
	union task_union *task_union;
	struct task_struct *tsk;
	unsigned long irqstate;

	id = alloc_id(id_request, start_state);
	if (id == ERROR_ID)
		goto fail_id_alloc;

	task_union = kmem_get_pages(TASK_ORDER);
	if (!task_union)
		goto fail_task_alloc;

	/*
	 * Initialize the new task. kmem_alloc() allocates zeroed memory
	 * so fields with an initial state of zero do not need to be explicitly
	 * initialized.
	 */
	tsk		= &task_union->task_info;
	tsk->id		= id;
	tsk->aspace	= aspace_acquire(start_state->aspace_id);
	tsk->state	= TASKSTATE_READY;
	tsk->uid	= start_state->uid;
	tsk->gid	= start_state->gid;
	tsk->cpu_id	= start_state->cpu_id;
	tsk->cpumask	= current->cpumask;

	hlist_node_init(&tsk->ht_link);
	list_head_init(&tsk->sched_link);

	if (name)
		strlcpy(tsk->name, name, sizeof(tsk->name));

	if (start_state->cpumask)
		cpumask_user2kernel(start_state->cpumask, &tsk->cpumask);

	/* Error checks */
	if (!tsk->aspace)
		goto fail_aspace;

	if (!cpus_subset(tsk->cpumask, current->cpumask))
		goto fail_cpumask;

	if ((tsk->cpu_id >= NR_CPUS) || !cpu_isset(tsk->cpu_id, tsk->cpumask))
		goto fail_cpu;

	/* Set the next CPU to use. This is used for the clone() syscall,
	 * which doesn't allow an explicit CPU to be specified. */
	tsk->next_cpu = next_cpu(tsk->cpu_id, tsk->cpumask);
	if (tsk->next_cpu == NR_CPUS)
		tsk->next_cpu = first_cpu(tsk->cpumask);

	/* Do architecture-specific initialization */
	if (arch_task_create(tsk, start_state, parent_regs))
		goto fail_arch;

	/* Add new task to a hash table, for quick lookups by ID */
	spin_lock_irqsave(&htable_lock, irqstate);
	htable_add(htable, tsk);
	spin_unlock_irqrestore(&htable_lock, irqstate);

	/* Add the new task to the target CPU's run queue */
	if (tsk->id != IDLE_TASK_ID)
		sched_add_task(tsk);

	return tsk;  /* Success! */

fail_arch:
fail_cpu:
fail_cpumask:
	aspace_release(tsk->aspace);
fail_aspace:
	kmem_free_pages(task_union, TASK_ORDER);
fail_task_alloc:
	free_id(id);
fail_id_alloc:
	return NULL;
}

int
task_create(id_t id_request, const char *name,
            const start_state_t *start_state, id_t *id)
{
	struct task_struct *tsk;

	/* Create and initialize a new task */
	tsk = __task_create(id_request, name, start_state, NULL);
	if (!tsk)
		return -EINVAL;
	if (id)
		*id = tsk->id;
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
		return -EFAULT;

	if (_start_state.aspace_id == KERNEL_ASPACE_ID)
		return -EINVAL;

	if (_start_state.cpumask) {
		if (copy_from_user(&_cpumask, _start_state.cpumask, sizeof(_cpumask)))
			return -EFAULT;
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
	/*
	 * Nothing to do, schedule() will be automatically
	 * called before returning to user-space
	 */
	return 0;
}

int
sys_task_yield(void)
{
	return task_yield();
}

int
task_get_cpu(id_t *cpu_id)
{
	*cpu_id = current->cpu_id;
	return 0;
}

int
sys_task_get_cpu(id_t __user *cpu_id)
{
	int status;
	id_t _cpu_id;

	if ((status = task_get_cpu(&_cpu_id)) != 0)
		return status;

	if (cpu_id && copy_to_user(cpu_id, &_cpu_id, sizeof(*cpu_id)))
		return -EFAULT;

	return 0;
}

int
task_get_cpumask(user_cpumask_t *cpumask)
{
	cpumask_kernel2user(&current->cpumask, cpumask);
	return 0;
}

int
sys_task_get_cpumask(user_cpumask_t __user *cpumask)
{
	int status;
	user_cpumask_t _cpumask;

	if ((status = task_get_cpumask(&_cpumask)) != 0)
		return status;

	if (cpumask && copy_to_user(cpumask, &_cpumask, sizeof(*cpumask)))
		return -EFAULT;

	return 0;
}

/**
 * Looks up an aspace object by ID and returns it with its spinlock locked.
 */
struct task_struct *
task_lookup(id_t id)
{
        struct task_struct *t;

        /* Lock the hash table, lookup aspace object by ID */
        spin_lock(&htable_lock);
        if ((t = htable_lookup(htable, &id)) == NULL) {
                spin_unlock(&htable_lock);
                return NULL;
        }

        /* Unlock the hash table, others may now use it */
        spin_unlock(&htable_lock);

        return t;
}

static void
kthread_trampoline(
	int		(*entry_point)(void *arg),
	void *		arg
)
{
	int status = entry_point(arg);
	task_exit(status);
}

struct task_struct *
kthread_create(
	int		(*entry_point)(void *arg),
	void *		arg,
	const char *	fmt,
	...
)
{
	char name[16];
	va_list ap;
	id_t id;

	va_start(ap, fmt);
	vsnprintf(name, sizeof(name)-1, fmt, ap);
	name[sizeof(name)-1] = '\0';
	va_end(ap);

	start_state_t state = {
		.aspace_id	= KERNEL_ASPACE_ID,
		.entry_point	= (vaddr_t) kthread_trampoline,
		.arg[0]		= (uintptr_t) entry_point,
		.arg[1]		= (uintptr_t) arg,
		.cpu_id		= this_cpu,
	};

	if (task_create(ANY_ID, name, &state, &id) != 0)
		return NULL;

	return task_lookup(id);
}
