#ifndef _X86_64_XCALL_H
#define _X86_64_XCALL_H

#include <arch/ptrace.h>

void arch_xcall_function_interrupt(struct pt_regs *regs, unsigned int vector);
void arch_xcall_reschedule_interrupt(struct pt_regs *regs, unsigned int vector);

#endif
