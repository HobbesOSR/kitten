#include <lwk/kernel.h>
#include <lwk/console.h>
#include <lwk/smp.h>

/**
 * Prints a message to the console.
 */
int printk(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	int rc = vprintk( fmt, args );
	va_end(args);
	return rc;
}


int printk_print_cpu_number;


int
vprintk(
	const char *		fmt,
	va_list			args
)
{
	int len;
	char buf[1024];
	char *p = buf;
	int remain = sizeof(buf);

	/* Start with a NULL terminated string */
	*p = '\0';

	/* Tack on the logical CPU ID */
	if( printk_print_cpu_number )
	{
		len = sprintf(p, "[%u]:", this_cpu);
		p      += len;
		remain -= len;
	}

	/* Construct the string... */
	len = vscnprintf(p, remain, fmt, args);

	/* Pass the string to the console subsystem */
	console_write(buf);

	/* Return number of characters printed */
	return len;
}

