/** \file
 * Interface to standard PC keyboards.
 *
 * This file hooks the keyboard interrupt and routes it the guest OS
 * if CONFIG_PALACIOS is defined, or otherwise throws away the input.
 *
 * It is in device class "late", which is done after most every other
 * device has been setup.
 */
#include <lwk/driver.h>
#include <lwk/signal.h>
#include <lwk/interrupt.h>
#include <arch/proto.h>
#include <arch/io.h>
#include <arch/idt_vectors.h>
#ifdef CONFIG_PALACIOS
#include <arch/palacios.h>
#endif


/**
 * Handle a keyboard interrupt.
 */
static irqreturn_t
do_keyboard_interrupt(
	unsigned int		vector,
	void *			unused
)
{
	const uint8_t KB_STATUS_PORT = 0x64;
	const uint8_t KB_DATA_PORT   = 0x60;
	const uint8_t KB_OUTPUT_FULL = 0x01;

	uint8_t status = inb(KB_STATUS_PORT);

	if ((status & KB_OUTPUT_FULL) == 0)
		return IRQ_NONE;

	uint8_t key = inb(KB_DATA_PORT);
#ifdef CONFIG_PALACIOS
	send_key_to_palacios(status, key);
#else
	printk("Keyboard Interrupt: status=%u, key=%u\n", status, key);
#endif
	return IRQ_HANDLED;
}


/**
 * Register handlers for standard PC devices.
 */
void
keyboard_init( void )
{
	request_irq(
		IRQ1_VECTOR,
		&do_keyboard_interrupt,
		0,
		"keyboard",
		NULL
	);
}


driver_init( "late", keyboard_init );
