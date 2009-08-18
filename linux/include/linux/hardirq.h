#ifndef __LINUX_HARDIRQ_H
#define __LINUX_HARDIRQ_H

#include <lwk/interrupt.h>

extern void
synchronize_irq(unsigned int irq);

#endif
