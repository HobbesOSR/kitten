#ifndef _ASM_PAT_H
#define _ASM_PAT_H

#include <linux/types.h>

static const int pat_enabled = 0;

extern int reserve_memtype(u64 start, u64 end,
		unsigned long req_type, unsigned long *ret_type);

extern int free_memtype(u64 start, u64 end);

#endif
