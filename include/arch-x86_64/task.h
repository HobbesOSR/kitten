#ifndef _ARCH_TASK_H
#define _ARCH_TASK_H

/**
 * Bits in the arch_task.flags field.
 */
#define _TF_NEW_TASK_BIT  0
#define _TF_USED_FPU_BIT  1

/**
 * Masks for the bits in the arch_task.flags field.
 */
#define TF_NEW_TASK   (1 << _TF_NEW_TASK_BIT)
#define TF_USED_FPU   (1 << _TF_USED_FPU_BIT)

#ifndef __ASSEMBLY__

#include <arch/pda.h>
#include <arch/page_table.h>

struct arch_mm {
	xpte_t	*page_table_root;
};

/**
 * Architecture-specific task information.
 */
struct arch_task {
	uint32_t		flags;		/* arch-dependent task flags */
	unsigned long		addr_limit;	/* task's virtual memory space is from [0,addr_limit) */
	struct thread_struct 	thread;
};

#endif
#endif
