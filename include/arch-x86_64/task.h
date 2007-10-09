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
 * Architecture-specific task information.
 */
struct arch_task {
	uint32_t		status;		/* thread-synchronous status */
	unsigned long		addr_limit;	/* task's virtual memory space is from [0,addr_limit) */
	struct thread_struct 	thread;
};

#endif
