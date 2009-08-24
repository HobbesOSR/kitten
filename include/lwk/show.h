#ifndef _LWK_SHOW_H
#define _LWK_SHOW_H

#include <lwk/ptrace.h>
#include <arch/show.h>

extern void show_memory(vaddr_t vaddr, size_t n);
extern void show_registers(struct pt_regs *regs);
extern void show_kstack(void);

#endif
