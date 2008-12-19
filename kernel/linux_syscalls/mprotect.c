#include <lwk/kernel.h>

long
sys_mprotect(
	unsigned long		start,
	size_t			len,
	unsigned long		prot
)
{
	return 0;
}
