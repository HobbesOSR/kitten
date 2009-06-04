/** \file
 * Architecture specific definitions for lwip
 */

#ifndef _lwip_sys_arch_h_
#define _lwip_sys_arch_h_

#include <lwk/types.h>
#include <lwk/spinlock.h>

// All lwip locks are protected by a higher lock.
// They are not truely atomic on their own.
typedef volatile int * sys_sem_t;

struct mbox
{
	int		read;
	int		write;
	int		size;
	void *		msgs[];
};


typedef struct mbox * sys_mbox_t;

#define SYS_SEM_NULL 0
#define SYS_MBOX_NULL 0

#define FD_ZERO(x) __FD_ZERO(x)
#define FD_ISSET(x,y) __FD_ISSET(x,y)


typedef int sys_thread_t;


typedef unsigned long sys_prot_t;

#endif
