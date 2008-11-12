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
#include <arch/proto.h>
#include <arch/io.h>
#include <arch/idt_vectors.h>
#ifdef CONFIG_PALACIOS
#include <arch/palacios.h>
#endif


/**
 * Handle a keyboard interrupt.
 */
static void
do_keyboard_interrupt(
	struct pt_regs *	regs,
	unsigned int		vector
)
{
	const uint8_t KB_STATUS_PORT = 0x64;
	const uint8_t KB_DATA_PORT   = 0x60;
	const uint8_t KB_OUTPUT_FULL = 0x01;

	uint8_t status = inb(KB_STATUS_PORT);

	if ((status & KB_OUTPUT_FULL) == 0)
		return;

	uint8_t key = inb(KB_DATA_PORT);
#ifdef CONFIG_PALACIOS
	send_key_to_palacios(status, key);
#else
	printk("Keyboard Interrupt: status=%u, key=%u\n", status, key);
#endif
}


/**
 * Register handlers for standard PC devices.
 */
void
keyboard_init( void )
{
	set_idtvec_handler( IRQ1_VECTOR, &do_keyboard_interrupt );
}


driver_init( "late", keyboard_init );
