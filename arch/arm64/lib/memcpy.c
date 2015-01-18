#include <lwk/types.h>
#include <lwk/print.h>

extern void
serial_num(unsigned long long num);

extern bool _can_print;

void *memcpy(void *dest, const void *src, size_t count)
{
	char *tmp = dest;
	const char *s = src;

	while (count--)
		*tmp++ = *s++;
	return dest;
}
