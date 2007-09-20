#include <lwk/kernel.h>

/**
 * Scream and die.
 */
void panic(const char * fmt, ...)
{
        static char buf[1024];
        va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	printk(KERN_EMERG "Kernel panic - not syncing: %s\n",buf);

	while (1) {}
}
