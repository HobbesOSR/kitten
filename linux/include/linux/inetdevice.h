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

extern struct net_device *ip_dev_find(struct net *net, __be32 addr);

#endif
