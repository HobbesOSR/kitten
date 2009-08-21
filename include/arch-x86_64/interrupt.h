#ifndef _ARCH_X86_64_INTERRUPT_H
#define _ARCH_X86_64_INTERRUPT_H

#include <arch/pda.h>
#include <arch/idt_vectors.h>

#define ARCH_NUM_IRQS	NUM_IDT_ENTRIES

static inline bool
in_interrupt(void)
{
	return (read_pda(irqcount) >= 0);
}

#endif
