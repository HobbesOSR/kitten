#include <lwk/kernel.h>
#include <lwk/console.h>


/** Prints a message to the console. */
int
printk(const char *fmt, ...)
{
	va_list args;
	int len;
	static char buf[1024];

	// Build the string in a buffer on the stack
	va_start(args, fmt);
	len = vscnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	// Pass the string to the console subsystem
	console_write(buf);

	// Return number of characters printed
	return len;
}

