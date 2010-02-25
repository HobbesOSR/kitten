#ifndef _LINUX_INETDEVICE_H
#define _LINUX_INETDEVICE_H

#include <linux/netdevice.h>
#include <linux/kernel.h>
#include <linux/arp_table.h>

extern struct net init_net;

struct in_device {
	struct net_device* dev;

	// lwk specific
	__be32	ifa_address;		
	struct arp_table*	arp_table;
};

static inline  struct net_device *ip_dev_find(struct net *net, __be32 addr)
{
    LINUX_DBG(FALSE,"addr=%#x\n",be32_to_cpu(addr));
    return NULL;
}

#endif
