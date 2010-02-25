
#ifndef _LINUX_SMP_LOCK_H
#define _LINUX_SMP_LOCK_H

#include <linux/kernel.h>

static inline void lock_kernel(void) 
{
	LINUX_DBG(FALSE,"\n");
}

static inline void unlock_kernel(void) 
{
	LINUX_DBG(FALSE,"\n");
}

#endif
