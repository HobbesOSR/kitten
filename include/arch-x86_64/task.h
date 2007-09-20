#ifndef _ARCH_TASK_H
#define _ARCH_TASK_H

#include <arch/pda.h>

/**
 * Architecture specific task information.
 */
struct arch_task {
	uint32_t	cpu;	/* task's current CPU */
};

/**
 * Returns a pointer to the currently executing task's task structure.
 * It is safe to call this from interrupt context but seldom does it
 * make sense.
 */
static inline struct task *current_task(void)
{
	struct task *tsk;
	tsk = (void *)(read_pda(kernelstack) + PDA_STACKOFFSET - TASK_SIZE);
	return tsk;
}

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
