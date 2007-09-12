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

	// Parse the LWK boot command line
	parse_args(
		"Parsing Arguments",
		lwk_command_line,
		__start___param,
		__stop___param - __start___param,
		NULL
	);

	// printk should work after this
	init_console();




	printk("%s\n", lwk_command_line);
	for (i = 0; i < 15; i++)
		printk("%d\n", i);

	while (1) {}
}

