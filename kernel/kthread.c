#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/aspace.h>
#include <lwk/sched.h>
#include <lwk/smp.h>


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
	struct task_struct *tsk;

	start_state_t state = {
		.task_id	= ANY_ID,
		.user_id	= 0,
		.group_id	= 0,
		.aspace_id	= KERNEL_ASPACE_ID,
		.cpu_id		= cpu_id,
		.entry_point	= (vaddr_t) kthread_trampoline,
		.use_args	= true,
		.arg[0]		= (uintptr_t) entry_point,
		.arg[1]		= (uintptr_t) arg,
	};

	strlcpy(state.task_name, name, sizeof(state.task_name));

	// Create and initialize the new kernel thread
	if ((tsk = __task_create(&state, NULL)) == NULL)
		return NULL;

	return tsk;
}


// Convenience function for creating a kernel thread.
// The kernel thread is created on the same CPU as the caller.
// Returns the task ID of the newly created kernel thread.
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
	struct task_struct * tsk = NULL;

	va_start(ap, fmt);
	vsnprintf(name, sizeof(name)-1, fmt, ap);
	name[sizeof(name)-1] = '\0';
	va_end(ap);

	tsk = __kthread_create_on_cpu(this_cpu, entry_point, arg, name);
	sched_add_task(tsk);

	return tsk;
}


// Convenience function for creating a kernel thread on a specific CPU.
// Returns the task ID of the newly created kernel thread.
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

int kthread_bind( struct task_struct* tsk, int cpu_id )
{
    /* Make sure target CPU is valid */
    if ((cpu_id >= NR_CPUS) || !cpu_isset(cpu_id, tsk->cpu_mask))
        return -EINVAL;

    /* Migrate to the target CPU */
    tsk->cpu_target_id = cpu_id;
    return 0;
}

int kthread_should_stop(void)
{
    return 0;
}
