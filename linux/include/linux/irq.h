#ifndef _LINUX_IRQ_H
#define _LINUX_IRQ_H

/*
 * Please do not include this file in generic code.  There is currently
 * no requirement for any architecture to implement anything held
 * within this file.
 *
 * Thanks. --rmk
 */

/*
 * Checks whether the interrupt can be requested by request_irq().
 * 
 * LWK Note: This appears to only be used by the PCI code to see if an IRQ
 *           can be shared.  We'd prefer to not have shared IRQs so always
 *           return false -- may need to revisit if this causes problems.
 */
static inline int
can_request_irq(unsigned int irq, unsigned long irqflags)
{
	return 0;
}

#endif /* _LINUX_IRQ_H */
