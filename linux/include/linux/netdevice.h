#ifndef _LINUX_NETDEVICE_H
#define _LINUX_NETDEVICE_H

#include <linux/if.h>
#include <linux/notifier.h>
#include <linux/lwk_stubs.h>
#include <net/net_namespace.h>
#include <linux/socket.h>
#include <linux/spinlock.h>
#include <linux/list.h>

extern rwlock_t	dev_base_lock;
#define for_each_netdev(net, d)         \
		list_for_each_entry(d, &(net)->dev_base_head, dev_list)


/* netdevice notifier chain */
#define NETDEV_UP       0x0001  /* For now you can't veto a device up/down */
#define NETDEV_DOWN     0x0002

struct ethtool_cmd {
	__u32   cmd;
	__u32   supported;     
	 __u16   speed;
};

struct net_device {
	char            name[IFNAMSIZ];

	unsigned char       addr_len;   /* hardware address length  */

	unsigned short type;
	/* Interface address info used in eth_type_trans() */
	unsigned char       dev_addr[MAX_ADDR_LEN]; /* hw address, (before bcast 
                            because most packets are unicast) */

	unsigned char       broadcast[MAX_ADDR_LEN];    /* hw bcast add */
	struct list_head        dev_list;
	atomic_t                refcnt ____cacheline_aligned_in_smp;


	unsigned int flags;
	unsigned int priv_flags;
	const struct ethtool_ops *ethtool_ops;
	unsigned                mtu; 
    int                     ifindex;
};

struct ethtool_ops {
        int     (*get_settings)(struct net_device *, struct ethtool_cmd *);
};

static inline void dev_put(struct net_device *dev)
{
	// this is called after ip_dev_find which doesn't allocate a structure
	//LINUX_DBG(FALSE,"\n");
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

static inline int netif_running(const struct net_device *dev)
{
	LINUX_DBG(TRUE,"\n");
	return 1;//test_bit(__LINK_STATE_START, &dev->state);
}
static inline int netif_oper_up(const struct net_device *dev)
{
        return 1;//(dev->operstate == IF_OPER_UP ||
                 //dev->operstate == IF_OPER_UNKNOWN /* backward compat */);
}

static inline void dev_hold(struct net_device *dev)
{
        atomic_inc(&dev->refcnt);
}


static inline struct net_device        *dev_get_by_index(struct net *net, int ifindex)
{
        LINUX_DBG(TRUE,"\n");
        return NULL;
}
extern int              dev_mc_add(struct net_device *dev, void *addr, int alen, int newonly);
extern int              dev_mc_delete(struct net_device *dev, void *addr, int alen, int all);

#endif
