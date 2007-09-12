#include <lwk/init.h>
#include <lwk/kernel.h>
#include <lwk/params.h>
#include <lwk/console.h>


/** Pristine copy of the LWK boot command line. */
char lwk_command_line[COMMAND_LINE_SIZE];


void
init()
{
	int i;

	// Pick up any boot-time parameters passed on the command line.
	parse_params(lwk_command_line);

	// printk should work after this
	init_console();

	printk("%s\n", lwk_command_line);
	for (i = 0; i < 15; i++)
		printk("%d\n", i);

	while (1) {}
}

