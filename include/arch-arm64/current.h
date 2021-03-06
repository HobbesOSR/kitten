#ifndef _ARM_64_CURRENT_H
#define _ARM_64_CURRENT_H

#if !defined(__ASSEMBLY__) 
//struct task_struct;

#include <arch/pda.h>

/**
 * In normal operation, the current task pointer is read directly from
 * the PDA.
 *
 * If the PDA has not been setup or is not available for some reason,
 * the slower get_current_via_RSP() must be used instead.  This is
 * sometimes necessary during the bootstrap process.
 */
static inline struct task_struct *
get_current(void) 
{ 
    struct task_struct *t = NULL;
    t = read_pda(pcurrent);
	return t;
} 
#define current get_current()

/**
 * Derives the current task pointer from the current value of the
 * stack pointer (RSP register).
 *
 * WARNING: Do not call this from interrupt context.  It won't work.
 *          It is only safe to call this from task context.
 */
static inline struct task_struct *
get_current_via_RSP(void)
{
	struct task_struct *tsk;
	__asm__("mov x10, sp\r\n"
		"and %0, x10, %1\r\n" 
	: "=r"(tsk) 
	: "ri"(~(TASK_SIZE - 1))
	:"x10");

//	printk("TaskPtr=%p\n", (void *)tsk);

	return tsk;
}

#else

#ifndef ASM_OFFSET_H
#include <arch/asm-offsets.h> 
#endif

#define GET_CURRENT(reg) movq %gs:(pda_pcurrent),reg

#endif

#endif /* !(_ARM_64_CURRENT_H) */
