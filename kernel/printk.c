#include <lwk/kernel.h>
#include <lwk/console.h>
#include <lwk/smp.h>

/**
 * Prints a message to the console.
 */
int printk(
	const char *		fmt,
	...
)
{
	va_list args;
	int chars_printed;

	va_start(args, fmt);
	chars_printed = vprintk(fmt, args);
	va_end(args);

	return chars_printed;
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


/** Format a data buffer as a series of hex bytes.
 */
const char *
hexdump(
	const void *		data_v,
	size_t			len
)
{
	int i;
	const uint8_t * const data = data_v;
	static char buf[ 32*3 + 5 ];

	size_t offset = 0;
	for( i=0 ; i<len && offset < sizeof(buf)-10 ; i++ )
	{
		offset += snprintf(
			buf+offset,
			sizeof(buf)-offset,
			"%02x ",
			data[i]
		);
	}

	if( i < len )
		snprintf(
			buf + offset,
			sizeof(buf)-offset,
			"..."
		);
	else
		buf[offset] = '\0';

	return buf;
}
