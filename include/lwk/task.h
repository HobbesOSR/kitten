#ifndef _LWK_TASK_H
#define _LWK_TASK_H

#include <lwk/types.h>
#include <arch/page.h>
#include <arch/task.h>

struct task {
	uint32_t		pid;	/* Process ID */
	uint32_t		tid;	/* Thread ID */
	uint32_t		uid;	/* User ID */
	uint32_t		cpu;	/* CPU task is executing on */

	struct arch_task	arch;	/* arch specific task info */
};

union task_union {
	struct task	task_info;
	unsigned long	stack[TASK_SIZE/sizeof(long)];
};

extern union task_union init_task_union;

#endif
