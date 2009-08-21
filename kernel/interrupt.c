/** \file
 * Architecture independent interrupt handling.
 *
 * These are for interrupts that do not depend on the
 * underlying hardware implementation, such as device drivers.
 * This that need access to the low-level register state should
 * use the architecture dependent functions like set_idtvec_handler().
 *
 * Additionally, this allows interrupt sharing if the handler does not
 * return a IRQ_HANDLED result.
 */

#include <lwk/interrupt.h>
#include <lwk/list.h>
#include <lwk/print.h>
#include <lwk/kmem.h>
#include <lwk/spinlock.h>
#include <arch/desc.h>
#include <arch/proto.h>

struct irq_desc {
	spinlock_t		lock;
	struct list_head	handlers;
};

struct handler_desc {
	struct list_head	link;
	irq_handler_t		handler;
	unsigned long		irqflags;
	const char *		devname;
	void *			dev_id;
};

static struct irq_desc irqs[NUM_IRQS];

static void
irq_dispatch(
	struct pt_regs *	regs,
	unsigned int		irq
)
{
	struct irq_desc *irq_desc = &irqs[irq];
	struct handler_desc *handler_desc;
	irqreturn_t status = IRQ_NONE;

	spin_lock(&irq_desc->lock);

	list_for_each_entry(handler_desc, &irq_desc->handlers, link) {
		status = handler_desc->handler(irq, handler_desc->dev_id);
		if (status == IRQ_HANDLED)
			break;
	}

	spin_unlock(&irq_desc->lock);

	if (status != IRQ_HANDLED) {
		printk(KERN_WARNING
			"%s: Unhandled interrupt %d (%x)\n",
			__func__,
			irq,
			irq
		);
	}
}

int
irq_request(
	unsigned int		irq,
	irq_handler_t		handler,
	unsigned long		irqflags,
	const char *		devname,
	void  *			dev_id
)
{
	struct irq_desc *irq_desc = &irqs[irq];
	struct handler_desc *handler_desc;
	unsigned long irqstate;

	if (irq >= NUM_IRQS)
		return -1;

	set_idtvec_handler(irq, irq_dispatch);

	handler_desc = kmem_alloc(sizeof(struct handler_desc));
	if (!handler_desc)
		return -1;

	*handler_desc = (typeof(*handler_desc)){
		.link		= LIST_HEAD_INIT(handler_desc->link),
		.handler	= handler,
		.irqflags	= irqflags,
		.devname	= devname,
		.dev_id		= dev_id,
	};

	spin_lock_irqsave(&irq_desc->lock, irqstate);
	list_add_tail(&handler_desc->link, &irq_desc->handlers);
	spin_unlock_irqrestore(&irq_desc->lock, irqstate);

	return 0;
}

void
irq_free(
	unsigned int		irq,
	void *			dev_id
)
{
	struct irq_desc *irq_desc = &irqs[irq];
	struct handler_desc *handler_desc;
	unsigned long irqstate;

	if (irq >= NUM_IRQS)
		return;

	spin_lock_irqsave(&irq_desc->lock, irqstate);

	list_for_each_entry(handler_desc, &irq_desc->handlers, link) {
		if (handler_desc->dev_id != dev_id)
			continue;
		list_del(&handler_desc->link);
		kmem_free(handler_desc);
		break;
	}

	spin_unlock_irqrestore(&irq_desc->lock, irqstate);
}

void __init
irq_init(void)
{
	struct irq_desc *irq_desc;
	int i;

	for (i = 0; i < NUM_IRQS; i++) {
		irq_desc = &irqs[i];
		spin_lock_init(&irq_desc->lock);
		list_head_init(&irq_desc->handlers);
	}
}
