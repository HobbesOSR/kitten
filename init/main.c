#include <lwk/init.h>
#include <lwk/kernel.h>
#include <lwk/params.h>
#include <lwk/console.h>

#include <lwk/driver.h>

/** Pristine copy of the LWK boot command line. */
char lwk_command_line[COMMAND_LINE_SIZE];


void
start_kernel()
{
	// Pick up any boot-time parameters passed on the command line.
	parse_params(lwk_command_line);

	// printk should work after this
	console_init();

	// Hello, Dave...
	printk(lwk_banner);
	printk("%s\n", lwk_command_line);

	// Do architecture specific initialization
	setup_arch();

	while (1) {}
}

