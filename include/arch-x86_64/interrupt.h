#ifndef _ARCH_X86_64_INTERRUPT_H
#define _ARCH_X86_64_INTERRUPT_H

#include <arch/pda.h>

static inline bool
in_interrupt(void)
{
	return (read_pda(irqcount) >= 0);
}

#endif
