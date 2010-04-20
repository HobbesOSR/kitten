#include <lwk/kernel.h>
#include <lwk/xcall.h>
#include <lwk/htable.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <lwk/sched.h>
#include <lwk/smp.h>

/**
 * Hash table used to lookup task structures by ID.
 */
static struct htable *htable;
static DEFINE_SPINLOCK(htable_lock);

/**
 * Where to start looking for ANY_ID kernel tasks and user tasks, respectively.
 */
static id_t ktask_next_id = KTASK_MIN_ID;
static id_t utask_next_id = UTASK_MIN_ID + 100;

int __init
task_subsys_init(void)
{
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

int sys_task_is_zombie(id_t id)
{
    struct task_struct *tsk = task_lookup(id);
    if ( tsk  == NULL ) return -EINVAL;
    return ( tsk->state == TASK_EXIT_ZOMBIE );
}

int
task_get_myid(id_t *id)
{
	*id = current->id;
	return 0;
}

static bool
ktask_id_ok(id_t id)
{
	return ((id >= KTASK_MIN_ID) && (id <= KTASK_MAX_ID));
}

static id_t
next_id_inc(id_t *next_id)
{
	if      (*next_id == KTASK_MAX_ID) *next_id = KTASK_MIN_ID;
	else if (*next_id == UTASK_MAX_ID) *next_id = UTASK_MIN_ID;
	else                               *next_id += 1;

	return *next_id;
}

static id_t
id_alloc_any(bool is_ktask)
{
	id_t *next_id = is_ktask ? &ktask_next_id : &utask_next_id;
	id_t stop_id = *next_id;
	id_t task_id;

	while (htable_lookup(htable, next_id) != NULL) {
		if (next_id_inc(next_id) == stop_id)
			return ERROR_ID;
	}

	task_id = *next_id;
	next_id_inc(next_id);

	return task_id;
}

static id_t
id_alloc_specific(id_t task_id, bool is_ktask)
{
	if (is_ktask && (ktask_id_ok(task_id) == false))
		return ERROR_ID;

	/* The idle task is a special case...
  	 * There is one idle task per CPU, each with the same task ID */
	if (is_ktask && (task_id == IDLE_TASK_ID))
		return task_id;

	if (htable_lookup(htable, &task_id) != NULL)
		return ERROR_ID;

	return task_id;
}

struct task_struct *
__task_create(
	const start_state_t *	start_state,
	const struct pt_regs *	parent_regs
)
{
	id_t task_id;
	union task_union *task_union;
	struct task_struct *tsk;
	unsigned long irqstate;
	bool is_ktask = (start_state->aspace_id == KERNEL_ASPACE_ID);

	spin_lock_irqsave(&htable_lock, irqstate);

	task_id = (start_state->task_id == ANY_ID)
			? id_alloc_any(is_ktask)
			: id_alloc_specific(start_state->task_id, is_ktask);

	if (task_id == ERROR_ID)
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
	tsk->id		= task_id;
	tsk->aspace	= aspace_acquire(start_state->aspace_id);
	tsk->state	= TASK_RUNNING;
	tsk->uid	= start_state->user_id;
	tsk->gid	= start_state->group_id;
	tsk->cpu_id	= start_state->cpu_id;
	tsk->cpumask	= cpumask_user2kernel(&start_state->cpumask);
	tsk->signal	= kmem_alloc(sizeof(struct signal_struct));
	strlcpy(tsk->name, start_state->task_name, sizeof(tsk->name));

	hlist_node_init(&tsk->ht_link);
	list_head_init(&tsk->sched_link);

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

	// We always clone files
	memcpy( tsk->files, current->files, sizeof(tsk->files) );

	/* Do architecture-specific initialization */
	if (arch_task_create(tsk, start_state, parent_regs))
		goto fail_arch;

	/* Add new task to a hash table, for quick lookups by ID */
	htable_add(htable, tsk);

	/* Setup aliases needed for Linux compatibility layer */
	tsk->comm = tsk->name;
	tsk->mm   = tsk->aspace;

	spin_unlock_irqrestore(&htable_lock, irqstate);
	return tsk;  /* Success! */

fail_arch:
fail_cpu:
fail_cpumask:
	aspace_release(tsk->aspace);
fail_aspace:
	kmem_free_pages(task_union, TASK_ORDER);
fail_task_alloc:
fail_id_alloc:
	spin_unlock_irqrestore(&htable_lock, irqstate);
	return NULL;
}

int __task_destroy( struct task_struct *tsk )
{
	__KDBG("tsk=%p\n",tsk);
	aspace_release(tsk->aspace);
	aspace_destroy(tsk->aspace->id);
	//idspace_free_id(idspace, tsk->id);
	kmem_free_pages(tsk, TASK_ORDER);
	return 0;
}

int sys_task_destroy( id_t id )
{   
    __KDBG("id=%d\n",id);
    struct task_struct *tsk = task_lookup(id);
    
    if ( tsk == NULL ) 
        return -EINVAL;
    
    return __task_destroy( tsk );
}


int
task_create(const start_state_t *start_state, id_t *task_id)
{
	struct task_struct *tsk;

	/* Create and initialize a new task */
	tsk = __task_create(start_state, NULL);
	if (!tsk)
		return -EINVAL;

	/* Add the new task to the target CPU's run queue */
	sched_add_task(tsk);

	if (task_id)
		*task_id = tsk->id;
	return 0;
}

int
task_exit(int status)
{
	/* Mark the task as exited...
	 * schedule() will remove it from the run queue */
	current->exit_status = status;
	current->state = TASK_EXIT_ZOMBIE;
	schedule(); /* task is dead, so this should never return */
	BUG();
	while (1) {}
}

int
sys_task_kill( id_t id, int signal)
{
    _KDBG("id=%d signal=%d\n",id,signal);

    struct task_struct *tsk = task_lookup(id);
    if ( tsk  == NULL ) return -EINVAL;
    tsk->state = TASK_EXIT_ZOMBIE;

    return 0;
}

int sys_tgkill( int tgid, int pid, int sig )
{
	_KDBG("tgid=%d pid=%d sig=%d\n",tgid,pid,sig);
	return sys_task_kill(pid,sig);
	//return sys_task_destroy(pid);
}

int
task_get_cpu(id_t *cpu_id)
{
	*cpu_id = current->cpu_id;
	return 0;
}

int
task_get_cpumask(user_cpumask_t *cpumask)
{
	*cpumask = cpumask_kernel2user(&current->cpumask);
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

static inline struct task_struct *
__kthread_create_on_cpu(
	id_t		cpu_id, 
	int		(*entry_point)(void *arg),
	void *		arg,
	const char *    name
)
{
	id_t task_id;

	start_state_t state = {
		.task_id	= ANY_ID,
		.user_id	= 0,
		.group_id	= 0,
		.aspace_id	= KERNEL_ASPACE_ID,
		.cpu_id		= cpu_id,
		.cpumask	= user_cpumask_of_cpu(cpu_id),
		.entry_point	= (vaddr_t) kthread_trampoline,
		.arg[0]		= (uintptr_t) entry_point,
		.arg[1]		= (uintptr_t) arg,
	};

	strlcpy(state.task_name, name, sizeof(state.task_name));

	if (task_create(&state, &task_id) != 0)
		return NULL;

	return task_lookup(task_id);
}

struct task_struct *
kthread_create(
	int		(*entry_point)(void *arg),
	void *		arg,
	const char *	fmt,
	...
)
{
	char name[32];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(name, sizeof(name)-1, fmt, ap);
	name[sizeof(name)-1] = '\0';
	va_end(ap);

	return __kthread_create_on_cpu(this_cpu, entry_point, arg, name);
}

struct task_struct *
kthread_create_on_cpu(
	id_t		cpu_id, 
	int		(*entry_point)(void *arg),
	void *		arg,
	const char *	fmt,
	...
)
{
	char name[32];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(name, sizeof(name)-1, fmt, ap);
	name[sizeof(name)-1] = '\0';
	va_end(ap);

	return __kthread_create_on_cpu(cpu_id, entry_point, arg, name);
}
