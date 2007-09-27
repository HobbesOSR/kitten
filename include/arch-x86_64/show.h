#ifndef _ASM_X86_64_SHOW_H
#define _ASM_X86_64_SHOW_H

#include <lwk/ptrace.h>

extern void printk_address(unsigned long address);
extern void show_registers(struct pt_regs *regs);

#endif
