#include <lwk/init.h>
#include <lwk/kernel.h>

/** Pristine copy of the LWK boot command line. */
char lwk_command_line[COMMAND_LINE_SIZE];

void
init()
{
	int i;
	printk("%s\n", lwk_command_line);
	for (i = 0; i < 15; i++)
		printk("%d\n", i);
	while (1) {}
}

