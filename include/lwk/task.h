#ifndef _LWK_TASK_H
#define _LWK_TASK_H

#include <lwk/types.h>
#include <lwk/spinlock.h>
#include <lwk/list.h>
#include <lwk/seqlock.h>
#include <lwk/signal.h>
#include <arch/atomic.h>
#include <arch/page.h>
#include <arch/processor.h>
#include <arch/task.h>
#include <arch/current.h>
#include <arch/mmu.h>

/**
 * Length of human readable task name.
 * Task name excludes path.
 */
#define TASK_NAME_LENGTH	16

/**
 * Flags for task_struct.flags field.
 */
#define PF_USED_MATH    0x00002000      /* if unset the fpu must be initialized before use */

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
 * system.  A task is a generic "context of execution"... it can either
 * represent a process, thread, or something in-between.
 */
struct task_struct {
	uint32_t		task_id;
	char			task_name[TASK_NAME_LENGTH];

	struct aspace		*aspace;    /* Address space the task runs in */
	struct sighand_struct   *sighand;   /* signal handler info */

	uint32_t		cpu;        /* CPU task is running on */
	unsigned long		ptrace;
	uint32_t		flags;

	struct arch_task	arch;       /* arch specific task info */
};

union task_union {
	struct task_struct	task_info;
	unsigned long		stack[TASK_SIZE/sizeof(long)];
};

extern union task_union init_task_union;
extern struct aspace init_aspace;

/**
 * Checks to see if a task structure is the init task.
 * The init task is the first user-space task created by the kernel.
 */
static inline int
is_init(struct task_struct *tsk)
{
	return (tsk->task_id == 1);
}

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

#endif
