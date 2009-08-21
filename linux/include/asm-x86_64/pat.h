#ifndef _ASM_PAT_H
#define _ASM_PAT_H

#include <linux/types.h>

static const int pat_enabled = 0;

static inline int
reserve_memtype(u64 start, u64 end, unsigned long req_type, unsigned long *new_type)
{
	/* This is identical to page table setting without PAT */
	if (new_type) {
		if (req_type == -1)
			*new_type = _PAGE_CACHE_WB;
		else
			*new_type = req_type & _PAGE_CACHE_MASK;
	}

	return 0;
}

static inline int
free_memtype(u64 start, u64 end)
{
	return 0;
}

#endif
