#ifndef _NET_LINUX_H
#define _NET_LINUX_H

#include <linux/kernel.h>

static inline void inet_get_local_port_range(int *low, int *high)
{
	*low = 1;
	*high = 1024;
}

static inline void ip_ib_mc_map(__be32 naddr, 
                    const unsigned char *broadcast, char *buf) 
{
	LINUX_DBG(TRUE,"\n");
}

#endif

