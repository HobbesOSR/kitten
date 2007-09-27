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

/**
 * Length of human readable task name.
 * Task name excludes path.
 */
#define TASK_NAME_LENGTH	16

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
 * represent a process, thread, or something inbetween.
 */
struct task {
	uint32_t		pid;	/* Process ID */
	uint32_t		tid;	/* Thread ID */
	uint32_t		uid;	/* User ID */
	uint32_t		cpu;	/* CPU task is executing on */

	unsigned long		ptrace;
	struct sighand_struct *	sighand;

	char			name[TASK_NAME_LENGTH];

	struct arch_task	arch;	/* arch specific task info */
};

union task_union {
	struct task	task_info;
	unsigned long	stack[TASK_SIZE/sizeof(long)];
};

extern union task_union init_task_union;

/**
 * Checks to see if a task structure is the init task.
 * The init task is the first user-space task created by the kernel.
 */
static inline int
is_init(struct task *tsk)
{
	return (tsk->pid == 1);
}

#endif
