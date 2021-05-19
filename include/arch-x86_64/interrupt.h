#ifndef _ARCH_X86_64_INTERRUPT_H
#define _ARCH_X86_64_INTERRUPT_H

#include <arch/pda.h>
#include <arch/idt_vectors.h>

#define ARCH_NUM_IRQS	NUM_IDT_ENTRIES

#define set_irq_handler set_idtvec_handler

static inline bool
arch_in_interrupt(void)
{
	return (read_pda(irqcount) >= 0);
}

#endif
