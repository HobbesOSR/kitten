#include <lwk/kernel.h>
#include <lwk/ptrace.h>
#include <lwk/task.h>
#include <lwk/aspace.h>
#include <lwk/sched.h>
#include <arch/uaccess.h>

#define CLONE_VM		0x00000100	/* set if VM shared between processes */
#define CLONE_FS		0x00000200	/* set if fs info shared between processes */
#define CLONE_FILES		0x00000400	/* set if open files shared between processes */
#define CLONE_SIGHAND		0x00000800	/* set if signal handlers and blocked signals shared */
#define CLONE_PTRACE		0x00002000	/* set if we want to let tracing continue on the child too */
#define CLONE_VFORK		0x00004000	/* set if the parent wants the child to wake it up on mm_release */
#define CLONE_PARENT		0x00008000	/* set if we want to have the same parent as the cloner */
#define CLONE_THREAD		0x00010000	/* Same thread group? */
#define CLONE_NEWNS		0x00020000	/* New namespace group? */
#define CLONE_SYSVSEM		0x00040000	/* share system V SEM_UNDO semantics */
#define CLONE_SETTLS		0x00080000	/* create a new TLS for the child */
#define CLONE_PARENT_SETTID	0x00100000	/* set the TID in the parent */
#define CLONE_CHILD_CLEARTID	0x00200000	/* clear the TID in the child */
#define CLONE_DETACHED		0x00400000	/* Unused, ignored */
#define CLONE_UNTRACED		0x00800000	/* set if the tracing process can't force CLONE_PTRACE on this clone */
#define CLONE_CHILD_SETTID	0x01000000	/* set the TID in the child */
#define CLONE_STOPPED		0x02000000	/* Start in stopped state */


long
sys_clone(
	unsigned long		flags,
	unsigned long		new_stack_ptr,
	int __user *		parent_tid_ptr,
	int __user *		child_tid_ptr,
	struct pt_regs *	parent_regs
)
{
	/* Only allow creating tasks that share everything with their parent */
	unsigned long required_flags =
		( CLONE_VM
		| CLONE_FS
		| CLONE_FILES
		| CLONE_SIGHAND
		| CLONE_THREAD
		| CLONE_SYSVSEM
		);

	if ((flags & required_flags) != required_flags) {
		printk(KERN_WARNING
		       "Unsupported clone() flags 0x%lx.\n", flags);
		return -ENOSYS;
	}

	/* Pick a CPU to spawn the new task on */
	id_t cpu_id = current->next_cpu;
	current->next_cpu = next_cpu(current->next_cpu, current->cpumask);
	if (current->next_cpu == NR_CPUS)
		current->next_cpu = first_cpu(current->cpumask);

	start_state_t start_state = {
		.task_id	= ANY_ID,
		.user_id	= current->uid,
		.group_id	= current->gid,
		.aspace_id	= current->aspace->id,
		.cpu_id		= cpu_id,
		.cpumask	= cpumask_kernel2user(&current->cpumask),
		.stack_ptr	= new_stack_ptr,
		.entry_point	= 0,    /* set automagically for clone() */
	};

	/* Name the new task something semi-sensible */
	snprintf(start_state.task_name, sizeof(start_state.task_name),
		 "%s_thread", strlen(current->name) ? current->name : "noname");

	struct task_struct *task = __task_create(&start_state, parent_regs);
	if (!task)
		return -EINVAL;

	/* Optionally initialize the task's set_child_tid and clear_child_tid */
	if ((flags & CLONE_CHILD_SETTID))
		task->set_child_tid = child_tid_ptr;
	if ((flags & CLONE_CHILD_CLEARTID))
		task->clear_child_tid = child_tid_ptr;

	/* Optionally write the new task's ID to user-space memory.
	 * It doesn't really matter if these fail. */
	int tid = task->id;
	if ((flags & CLONE_PARENT_SETTID))
		put_user(tid, parent_tid_ptr);
	if ((flags & CLONE_CHILD_SETTID))
		put_user(tid, child_tid_ptr);

	/* Add the new task to the target CPU's run queue */
	sched_add_task(task);
		
	return task->id;
}
