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

struct irq_list
{
	struct list_head	link;
	irq_handler_t		handler;
	unsigned long		irqflags;
	const char *		devname;
	void *			dev_id;
};

#define MAX_IRQ	256
static rwlock_t irq_lock = RW_LOCK_UNLOCKED;
static struct list_head irqs[ MAX_IRQ ];

static void
chain_irq(
	struct pt_regs *	regs,
	unsigned int		vector
)
{
	read_lock_irq( &irq_lock );

	int handled = 0;
	struct irq_list * handler;
	list_for_each_entry( handler, &irqs[vector], link )
	{
		irqreturn_t rc = handler->handler( vector, handler->dev_id );
		if( rc == IRQ_HANDLED )
		{
			handled = 1;
			break;
		}
	}

	if( !handled )
		printk( KERN_WARNING
			"%s: Unhandled interrupt %d (%x)\n",
			__func__,
			vector,
			vector
		);

	read_unlock( &irq_lock );
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
	if( irq > MAX_IRQ )
		return -1;

	set_idtvec_handler( irq, chain_irq );

	struct irq_list * entry = kmem_alloc( sizeof(*entry) );
	if( !entry )
		return -1;

	*entry = (typeof(*entry)){
		.link		= LIST_HEAD_INIT( entry->link ),
		.handler	= handler,
		.irqflags	= irqflags,
		.devname	= devname,
		.dev_id		= dev_id,
	};

	write_lock( &irq_lock );
	list_add_tail( &irqs[irq], &entry->link );
	write_unlock( &irq_lock );

	printk( "%s: %s: vector %d\n", __func__, devname, irq );

	return 0;
}


void
irq_free(
	unsigned int		irq,
	void *			dev_id
)
{
	if( irq > MAX_IRQ )
		return;

	write_lock( &irq_lock );

	struct irq_list * handler;
	list_for_each_entry( handler, &irqs[ irq ], link )
	{
		if( handler->dev_id != dev_id )
			continue;
		list_del( &handler->link );
		kmem_free( handler );
		break;
	}

	write_unlock( &irq_lock );
}


void __init
irq_init( void )
{
	int i;
	for( i=0 ; i<MAX_IRQ ; i++ )
		list_head_init( &irqs[i] );
}
