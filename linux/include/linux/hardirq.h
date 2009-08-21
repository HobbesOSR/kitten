#ifndef __LINUX_HARDIRQ_H
#define __LINUX_HARDIRQ_H

#include <lwk/interrupt.h>

static inline void
synchronize_irq(unsigned int irq)
{
	irq_synchronize(irq);
}

#endif
