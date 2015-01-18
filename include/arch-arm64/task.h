#ifndef _ARCH_X86_64_TASK_H
#define _ARCH_X86_64_TASK_H


// Bits in the arch_task.flags field
#define TF_NEW_TASK_BIT		0
#define TF_SIG_PENDING_BIT	1
#define TF_NEED_RESCHED_BIT	2


// Masks for the bits in the arch_task.flags field
#define TF_NEW_TASK_MASK	(1 << TF_NEW_TASK_BIT)
#define TF_SIG_PENDING_MASK	(1 << TF_SIG_PENDING_BIT)
#define TF_NEED_RESCHED_MASK	(1 << TF_NEED_RESCHED_BIT)


#ifndef __ASSEMBLY__


#include <arch/pda.h>
#include <arch/page_table.h>


struct arch_mm {
	xpte_t	*page_table_root;
};


// Architecture-specific task information
struct arch_task {
	uint32_t		flags;		// arch-dependent task flags
	unsigned long		addr_limit;	// task's virtual memory space is from [0,addr_limit)
	struct thread_struct 	thread;
};


#endif
#endif
