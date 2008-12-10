/** \file
 * Kernel and user tasks structures
 *
 * Both kernel threads and user tasks are represented by the same
 * task structures.
 */
#ifndef _LWK_TASK_H
#define _LWK_TASK_H

#include <lwk/types.h>
#include <lwk/idspace.h>
#include <lwk/cpumask.h>

/** \group User task IDs
 * Valid user-space created task IDs are in interval
 * [TASK_MIN_ID, TASK_MAX_ID].
 * @{
 */
#define TASK_MIN_ID                    0
#define TASK_MAX_ID                    4094

/**
 * The task ID to use for the init_task.
 * Put it at the top of the space to keep it out of the way.
 */
#define INIT_TASK_ID                   TASK_MAX_ID
//@}


/** \group Task states
 *
 * Tasks must be in one of the following states:
 */
#define TASKSTATE_READY                (1 << 0)
#define TASKSTATE_UNINTERRUPTIBLE      (1 << 1)
#define TASKSTATE_INTERRUPTIBLE        (1 << 2)
#define TASKSTATE_EXIT_ZOMBIE          (1 << 3)
typedef unsigned int taskstate_t;
//@}

/** \group Wait states
 *
 * Events that tasks may wait for and be sent.
 * @{
 */
#define LWKEVENT_CHILD_TASK_EXITED     (1 << 0)
#define LWKEVENT_PORTALS_EVENT_POSTED  (1 << 1)
typedef unsigned long event_t;
//@}

/**
 * Initial conditions to use for new task.
 */
typedef struct {
	uid_t                  uid;
	uid_t                  gid;
	id_t                   aspace_id;
	vaddr_t                stack_ptr;   //!< Ignored for kernel tasks
	vaddr_t                entry_point;
	uintptr_t              arg[4];      //!< Args to pass to entry_point()
	id_t                   cpu_id;
	const user_cpumask_t * cpumask;
} start_state_t;

/**
 * \group Core task management API.
 *
 * These are accessible from both kernel-space and user-space (via syscalls).
 *
 * @{
 */
extern int task_get_myid(id_t *id);
extern int task_create(id_t id_request, const char *name,
                       const start_state_t *start_state, id_t *id);
extern int task_exit(int status);

/** Yield the CPU from a user task.
 *
 * \note To yield a kernel thread, call schedule() instead.
 * This is effectively a NOP.
 */
extern int task_yield(void);

/** Returns the CPU that the task is currently executing on. */
extern int task_get_cpu(id_t *cpu_id);

/** Returns a mask representing the CPUs this task may execute on. */
extern int task_get_cpumask(user_cpumask_t *cpumask);
//@}

#ifdef __KERNEL__

#include <lwk/types.h>
#include <lwk/init.h>
#include <lwk/spinlock.h>
#include <lwk/list.h>
#include <lwk/seqlock.h>
#include <lwk/signal.h>
#include <lwk/idspace.h>
#include <arch/atomic.h>
#include <arch/page.h>
#include <arch/processor.h>
#include <arch/task.h>
#include <arch/current.h>
#include <arch/mmu.h>

/** \group Flags for task_struct.flags field.
 * @{
 */
#define PF_USED_MATH    0x00002000      /**< if unset the fpu must be initialized before use */

//@}


/**
 * Signal handler structure.
 */
struct sighand_struct {
	atomic_t		count;
	struct k_sigaction	action[_NSIG];
	spinlock_t		siglock;
	struct list_head        signalfd_list;
};

/**
 * Task structure (aka Process Control Block).
 * There is one of these for each OS-managed thread of execution in the
 * system.  A task is a generic "context of execution"... it either
 * represents a user-level process, user-level thread, or kernel thread.
 */
struct task_struct {
	id_t                    id;              /* The task's ID */
	char                    name[16];        /* The task's name */
	struct hlist_node       ht_link;         /* Task hash table linkage */

	taskstate_t             state;           /* The task's current state */

	uid_t                   uid;             /* user ID */
	gid_t                   gid;             /* group ID */

	struct aspace *         aspace;          /* Address space task is in */
	struct sighand_struct * sighand;         /* signal handler info */

	cpumask_t               cpumask;         /* CPUs this task may migrate
	                                            to and create tasks on */
	id_t                    cpu_id;          /* CPU this task is bound to */

	struct list_head        sched_link;      /* For per-CPU scheduling lists */
	bool                    sched_irqs_on;   /* IRQs on at schedule() entry? */

	unsigned long		ptrace;
	uint32_t		flags;

	int                     exit_status;     /* Reason the task exited */

	struct arch_task	arch;            /* arch specific task info */
};

union task_union {
	struct task_struct	task_info;
	unsigned long		stack[TASK_SIZE/sizeof(long)];
};

extern union task_union bootstrap_task_union;
extern struct aspace bootstrap_aspace;

/** \group Task IDs
 * Valid task IDs are in interval [__TASK_MIN_ID, __TASK_MAX_ID].
 * The kernel has one reserved ID that the user can not allocate
 * reserved for the idle task.
 * @{
 */
#define __TASK_MIN_ID   TASK_MIN_ID
#define __TASK_MAX_ID   TASK_MAX_ID+1  /**< +1 for IDLE_TASK_ID */

/**
 * ID of the idle task.
 */
#define IDLE_TASK_ID    TASK_MAX_ID+1
//@}

/**
 * Checks to see if a task structure is the init task.
 * The init task is the first user-space task created by the kernel.
 */
static inline int
is_init(struct task_struct *tsk)
{
	return (tsk->id == 1);
}

/** \group Floating Point Math flags.
 *
 * To save time during context switches, the floating point math
 * state is only stored if the PF_USED_MATH flag is set in the task
 * structure.
 *
 * @{
 */
#define clear_stopped_child_used_math(child) do { (child)->flags &= ~PF_USED_MATH; } while (0)
#define set_stopped_child_used_math(child) do { (child)->flags |= PF_USED_MATH; } while (0)
#define clear_used_math() clear_stopped_child_used_math(current)
#define set_used_math() set_stopped_child_used_math(current)
#define conditional_stopped_child_used_math(condition, child) \
        do { (child)->flags &= ~PF_USED_MATH, (child)->flags |= (condition) ? PF_USED_MATH : 0; } while (0)
#define conditional_used_math(condition) \
        conditional_stopped_child_used_math(condition, current)
#define copy_to_stopped_child_used_math(child) \
        do { (child)->flags &= ~PF_USED_MATH, (child)->flags |= current->flags & PF_USED_MATH; } while (0)
/* NOTE: this will return 0 or PF_USED_MATH, it will never return 1 */
#define tsk_used_math(p) ((p)->flags & PF_USED_MATH)
#define used_math() tsk_used_math(current)

// @}

extern int __init task_subsys_init(void);

extern int arch_task_create(struct task_struct *task,
                            const start_state_t *start_state);


/** \group Syscall wrappers for task creation
 *
 * \todo Make these static and use syscall_register() to install
 * the handlers at run time?
 *
 * @{
 */
extern int sys_task_get_myid(id_t __user *id);
extern int sys_task_create(id_t id_request, const char __user *name,
                           const start_state_t __user *start_state,
                           id_t __user *id);
extern int sys_task_exit(int status);
extern int sys_task_yield(void);
extern int sys_task_get_cpu(id_t __user *cpu_id);
extern int sys_task_get_cpumask(user_cpumask_t __user *cpumask);
//@}

extern int __task_reserve_id(id_t id);
extern int __task_create(id_t id, const char *name,
                         const start_state_t *start_state,
                         struct task_struct **task);

extern struct task_struct *task_lookup(id_t id);

/** Convenience function for creating a kernel thread.
 *
 * The kernel thread is created on the same CPU as the caller.
 * \returns The task ID of the new kernel thread.
 */
extern id_t
kthread_create(
	void		(*entry_point)(void *arg),
	void *		arg,
	const char *	name
);

#endif
#endif
