#ifndef _ARM64_INTERRUPT_H
#define _ARM64_INTERRUPT_H

#include <arch/pda.h>
#include <arch/irq_vectors.h>

#define ARCH_NUM_IRQS	NUM_IRQ_ENTRIES

static inline bool
arch_in_interrupt(void)
{
	return (read_pda(irqcount) >= 0);
}

#endif
