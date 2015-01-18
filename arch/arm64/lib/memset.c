/* Normally compiler builtins are used, but sometimes the compiler calls out
   of line code. Based on asm-i386/string.h.
 */
#define _STRING_C
#include <lwk/string.h>

#undef memset
void *memset(void *dest, int c, size_t count)
{
	char *p = (char *) dest + count;
	while (count--)
		*--p = c;
	return dest;
} 
