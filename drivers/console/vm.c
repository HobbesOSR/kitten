/* Console Driver when Kitten is running as a Palacios guest
 * Jack Lange (jarusl@cs.northwestern.edu)
 */

#include <lwk/delay.h>
#include <lwk/driver.h>
#include <lwk/console.h>
#include <arch/io.h>
#include <lwk/interrupt.h>


/**
 * IO port address of the vm console.
 *
 * \note This should be set to an unused port.
 */
static unsigned int port = 0xc0c0;


/** Set when the vm console has been initialized. */
static int initialized = 0;


/**
 * Prints a single character to the vm console port.
 */
static void vm_cons_putc(struct console *con, unsigned char c)
{
	// Slam the 8 bits down the 1 bit pipe... meeeooowwwy!
	outb(c, port);
}


/**
 * Writes a string to the vm console port.
 */
static void vm_cons_write(struct console *con, const char *str)
{
	unsigned char c;
	while ((c = *str++) != '\0') {
		vm_cons_putc(con, c);
	}
}


/**
 * Reads a single character from the vm console port.
 */
static char vm_cons_getc(struct console *con)
{
	return inb_p(port);
}


/**
 * Console device for use by guest virtual machines.
 */
static struct console vm_console = {
	.name		= "Virtual Machine Console",
	.write		= vm_cons_write,
	.poll_get_char	= vm_cons_getc,
	.poll_put_char	= vm_cons_putc
};


/**
 * Initializes and registers the vm console driver.
 */
int vm_console_init(void) {
	if (initialized) {
		printk(KERN_ERR "VM console already initialized.\n");
		return -1;
	}

	console_register(&vm_console);
	initialized = 1;

	return 0;
}


driver_init("console", vm_console_init);
driver_param(port, uint);
