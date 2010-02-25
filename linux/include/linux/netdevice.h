#ifndef _LINUX_NETDEVICE_H
#define _LINUX_NETDEVICE_H

#include <linux/if.h>
#include <linux/notifier.h>
#include <linux/lwk_stubs.h>
#include <net/net_namespace.h>
#include <linux/socket.h>

struct net_device {
	char            name[IFNAMSIZ];

	unsigned char       addr_len;   /* hardware address length  */

	unsigned short type;
	/* Interface address info used in eth_type_trans() */
	unsigned char       dev_addr[MAX_ADDR_LEN]; /* hw address, (before bcast 
                            because most packets are unicast) */

	unsigned char       broadcast[MAX_ADDR_LEN];    /* hw bcast add */

	unsigned int flags;
	unsigned int priv_flags;
};

static inline void dev_put(struct net_device *dev)
{
	LINUX_DBG(TRUE,"\n");
}

static inline int register_netdevice_notifier(struct notifier_block *nb)
{
	LINUX_DBG(FALSE,"\n");
	return 0;
}

static inline int unregister_netdevice_notifier(struct notifier_block *nb)
{
	LINUX_DBG(TRUE,"\n");
	return 0;
}

static inline struct net *dev_net(const struct net_device *dev)
{
	LINUX_DBG(TRUE,"\n");
	return NULL;
}

#endif
