#ifndef _LWK_INTERRUPT_H
#define _LWK_INTERRUPT_H

#include <arch/interrupt.h>

/**
 * The range of valid IRQs is assumed to be [0, NUM_IRQS).
 */
#define NUM_IRQS ARCH_NUM_IRQS

/**
 * Returns true if the caller is in interrupt context, false otherwise.
 */
static inline bool
in_interrupt(void)
{
	return arch_in_interrupt();
}

/**
 * IRQ handler return type.
 *
 *    IRQ_NONE means we didn't handle the interrupt.
 *    IRQ_HANDLED means we did handle the interrupt.
 *    IRQ_RETVAL(x) returns IRQ_HANDLED if x is non-zero, IRQ_NONE otherwise.
 */
typedef int irqreturn_t;
#define IRQ_NONE	(0)
#define IRQ_HANDLED	(1)
#define IRQ_RETVAL(x)	((x) != 0)

/**
 * IRQ handler prototype.
 */
typedef irqreturn_t (*irq_handler_t)(int irq, void *dev_id);

/**
 * Registers an interrupt handler.
 *
 * Linux compatible interface.
 */
extern int
irq_request(
	unsigned int		irq,
	irq_handler_t		handler,
	unsigned long		irqflags,
	const char *		devname,
	void  *			dev_id
);

/**
 * Requests a free IRQ vector and registers an interrupt handler for it
 *
 * Returns the IRQ allocated or -1 if there is no available IRQ
 */
extern int
irq_request_free_vector(
	irq_handler_t		handler,
	unsigned long		irqflags,
	const char *		devname,
	void  *			dev_id
);

/**
 * Unregisters an interrupt handler.
 */
extern void
irq_free(
	unsigned int		irq,
	void *			dev_id
);

/**
 * Synchronizes an IRQ.
 *
 * This synchronizes the specified IRQ with other CPUs, ensuring
 * that any inprogress handlers at the time of entry have exited.
 * Note this is a one time synchronization and does not prevent the
 * IRQ from being handled in the future... that's up to the caller.
 */
extern void
irq_synchronize(
	unsigned int		irq
);

extern void
irq_init( void );

#endif
