/*
 * lwk/arch/x86_64/lib/extable.c
 */

#include <lwk/init.h>
#include <arch/uaccess.h>

/* Simple binary search */
const struct exception_table_entry * __init
search_extable(const struct exception_table_entry *first,
	       const struct exception_table_entry *last,
	       unsigned long value)
{
	/* Work around a B stepping K8 bug */
	if ((value >> 32) == 0)
		value |= 0xffffffffUL << 32; 

        while (first <= last) {
		const struct exception_table_entry *mid;
		long diff;

		mid = (last - first) / 2 + first;
		diff = mid->insn - value;
                if (diff == 0)
                        return mid;
                else if (diff < 0)
                        first = mid+1;
                else
                        last = mid-1;
        }
        return NULL;
}
