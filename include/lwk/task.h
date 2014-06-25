/* Copyright (c) 2007-2010 Sandia National Laboratories */

#ifndef _LWK_TASK_H
#define _LWK_TASK_H

#include <lwk/types.h>
#include <lwk/idspace.h>
#include <lwk/cpumask.h>

// Setting start_state.entry_point to USE_PARENT_IP will cause the new task
// to begin executing at the current value of the caller's instruction pointer. 
#define USE_PARENT_IP		0


// Specifies the initial conditions to use when spawning a new task
typedef struct {
	id_t			task_id;
	char			task_name[32];

	id_t			user_id;	// User ID the task executes as
	id_t			group_id;	// Group ID the task executes as
	id_t			aspace_id;	// Address space the task executes in
	id_t			cpu_id;		// CPU ID the task starts executing on

#ifdef CONFIG_SCHED_EDF
	struct {
		uint64_t                slice;          // EDF Sched slice
		uint64_t                period;         // EDF Sched period
	} edf;
#endif
	vaddr_t			stack_ptr;	// Ignored for kernel tasks
	vaddr_t			entry_point;	// Instruction address to start executing at

	int			use_args;	// If true, pass args to entry_point()
	uintptr_t		arg[4];		// Args to pass to entry_point()
} start_state_t;


// Begin core task management API
// These are accessible from both kernel-space and user-space (via syscalls)

extern int
task_create(
	const start_state_t *	start_state,
	id_t *			task_id
);

extern int
task_switch_cpus(
	id_t			cpu_id
);

// End core task management API


#ifdef __KERNEL__

#include <lwk/types.h>
#include <lwk/init.h>
#include <lwk/list.h>
#include <lwk/rbtree.h>
#include <lwk/seqlock.h>
#include <lwk/idspace.h>
#include <lwk/rlimit.h>
#include <lwk/time.h>
#include <lwk/signal.h>
#include <arch/atomic.h>
#include <arch/page.h>
#include <arch/processor.h>
#include <arch/task.h>
#include <arch/current.h>
#include <arch/mmu.h>
#include <arch/ptrace.h>


// Symbolic names for well-known kernel task IDs
#define IDLE_TASK_ID		0


// Tasks must be in one of the following states:
#define TASK_RUNNING		(1 << 0)
#define TASK_INTERRUPTIBLE	(1 << 1)
#define TASK_UNINTERRUPTIBLE	(1 << 2)
#define TASK_STOPPED		(1 << 3)
#define TASK_EXITED		(1 << 4)
typedef unsigned int taskstate_t;


// Some commonly used combinations of task states:
#define TASK_NORMAL		(TASK_INTERRUPTIBLE | TASK_UNINTERRUPTIBLE)
#define TASK_ALL		(TASK_NORMAL)

struct fdTable;

// Task structure (aka Process Control Block).
// There is one of these for each OS-managed thread of execution in the
// system.  A task is a generic "context of execution"... it either
// represents a user-level process, user-level thread, or kernel thread.
struct task_struct {
	id_t			id;		// The task's ID, aspace local
	char			name[32];	// The task's name

	taskstate_t		state;		// The task's current state

	id_t			uid;		// user ID
	id_t			gid;		// group ID

	struct aspace *		aspace;		// Address space task is in
	struct list_head	aspace_link;	// Linkage for aspace.task_list

	struct sigpending	sigpending;	// For task-specific "synchronous" signals
	sigset_t		sigblocked;	// Mask of blocked signals

	id_t			cpu_id;		// CPU this task is executing on
	cpumask_t		cpu_mask;	// CPUs this task may migrate to
	id_t			cpu_target_id;	// CPU this task should migrate to


	int __user *		set_child_tid;	// CLONE_CHILD_SETTID
	int __user *		clear_child_tid;// CLONE_CHILD_CLEARTID

	unsigned long		ptrace;
	uint32_t		flags;

	int			exit_status;	// Reason the task exited

	struct arch_task	arch;		// arch specific task info
	struct fdTable*		fdTable;
	struct list_head	migrate_link;	// For per-CPU scheduling lists
	struct task_rr {
		struct list_head sched_link;
	} rr;
#ifdef CONFIG_SCHED_EDF
        struct task_edf {
		struct rb_node 	node;
		struct list_head sched_link;
		uint64_t	period;
		uint64_t	slice;
		int 		cpu_reservation;
		uint64_t	curr_deadline;
		uint64_t	last_wakeup;
		uint64_t	used_time;
		int		miss_deadlines;
		uint64_t	print_miss_deadlines;
		int		extra_time;
	} edf;       	// EDF Scheduler task structure
#endif
	bool			sched_irqs_on;	// IRQs on at schedule() entry?
	// Stuff needed for the Linux compatibility layer
	char *			comm;		// The task's name
	struct aspace *		mm;		// Address space task is in

	/* List of struct preempt_notifier */
	struct hlist_head       preempt_notifiers;
};


union task_union {
	struct task_struct	task_info;
	unsigned long		stack[TASK_SIZE/sizeof(long)];
};


extern union task_union bootstrap_task_union;
extern struct aspace bootstrap_aspace;


extern int __init task_subsys_init(void);


extern int
arch_task_create(
	struct task_struct *	task,
	const start_state_t *	start_state,
	const struct pt_regs *	parent_regs
);


// Syscall wrappers for task creation
extern int sys_task_create(const start_state_t __user *start_state,
                           id_t __user *task_id);


extern int sys_task_switch_cpus(id_t cpu_id);


extern struct task_struct *
__task_create(
	const start_state_t *	start_state,
	const struct pt_regs *	parent_regs
);


extern void
task_exit(
	int	exit_status
);


extern void
task_exit_group(
	int	exit_status
);


#endif
#endif
