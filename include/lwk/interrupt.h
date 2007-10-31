#ifndef _LWK_INTERRUPT_H
#define _LWK_INTERRUPT_H

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
#define IRQ_RETVAL	((x) != 0)

/**
 * IRQ handler prototype.
 */
typedef irqreturn_t (*irq_handler_t)(unsigned int irq, void *dev_id);

/**
 * Registers an interrupt handler.
 */
extern int request_irq(unsigned int	irq,
                       irq_handler_t	handler,
                       unsigned long	irqflags,
                       const char 	*devname,
                       void 		*dev_id);

/**
 * Unregisters an interrupt handler.
 */
extern void free_irq(unsigned int irq, void *dev_id);

#endif
