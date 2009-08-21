#ifndef _LWK_INTERRUPT_H
#define _LWK_INTERRUPT_H

#include <arch/interrupt.h>

/**
 * Each architecture must define the number of IRQs supported.
 * The range of valid IRQs is assumed to be [0, NUM_IRQS).
 */
#define NUM_IRQS	ARCH_NUM_IRQS

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
 * Unregisters an interrupt handler.
 */
extern void
irq_free(
	unsigned int		irq,
	void *			dev_id
);


extern void
irq_init( void );


#endif
