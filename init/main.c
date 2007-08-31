#include <lwk/init.h>
#include <lwk/kernel.h>
#include <lwk/screen_info.h>
#include <arch/bootsetup.h>

/** Pristine copy of the LWK boot command line. */
char lwk_command_line[COMMAND_LINE_SIZE];

/** Information about the video screen. */
struct screen_info screen_info;

void
init()
{
	printk("%s", lwk_command_line);
	while (1) {}
}
