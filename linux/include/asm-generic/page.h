#ifndef __LINUX_ASM_COMMON_PAGE_H
#define __LINUX_ASM_COMMON_PAGE_H

#include <lwk/compiler.h>
#include <lwk/string.h>

#define clear_page(page)	memset((void *)(page), 0, PAGE_SIZE)
#define copy_page(to,from)	memcpy((void *)(to), (void *)(from), PAGE_SIZE)

/* Pure 2^n version of get_order */
static __inline__ __attribute_const__ int get_order(unsigned long size)
{
	int order;

	size = (size - 1) >> (PAGE_SHIFT - 1);
	order = -1;
	do {
		size >>= 1;
		order++;
	} while (size);
	return order;
}

#endif
