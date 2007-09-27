#ifndef _ARCH_TASK_H
#define _ARCH_TASK_H

#include <arch/pda.h>

/**
 * Flags for arch_task.status field.
 */
#define TS_USEDFPU	0x0001	/* FPU was used by this task this quantum (SMP) */
#define TS_COMPAT	0x0002	/* 32bit syscall active */
#define TS_POLLING	0x0004	/* true if in idle loop and not sleeping */

/**
 * Architecture specific task information.
 */
struct arch_task {
	uint32_t		status;		/* thread-synchronous status */
	struct thread_struct 	thread;




	int cpu;
};

/**
 * Do not call this in interrupt context.  Interrupts are
 * handled using their own stack... i.e., there is no task
 * structure at the bottom of the stack.
 */
static inline struct task *stack_to_task(void)
{
	struct task *tsk;
	__asm__("andq %%rsp,%0; ":"=r" (tsk) : "0" (~(TASK_SIZE - 1)));
	return tsk;
}

#endif
