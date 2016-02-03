/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef	_LINUX_NETDEVICE_H_
#define	_LINUX_NETDEVICE_H_

#include <linux/types.h>


/*
#include <net/if_types.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
*/

#include <lwk/if.h>
#include <lwip/netif.h>
#include <lwip/igmp.h>


#include <linux/completion.h>
#include <linux/device.h>
#include <linux/ethtool.h>
#include <linux/workqueue.h>
#include <linux/net.h>
#include <linux/notifier.h>


extern rwlock_t dev_base_lock;
#define for_each_netdev(net, d)						\
	list_for_each_entry(d, &(net)->dev_base_head, dev_list)

struct net {
	struct list_head        dev_base_head;
};

#define	MAX_ADDR_LEN		20

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

        void * priv_data;
};

struct ethtool_cmd {
	u32 cmd;
	u32 supported;
	u16 speed;
};


struct ethtool_ops {
        int     (*get_settings)(struct net_device *, struct ethtool_cmd *);
};


extern struct net init_net;


void rtnl_lock(void);
void rtnl_unlock(void);


#define	dev_hold(d)	
#define	dev_put(d)	

#define	netif_carrier_ok(dev)	netif_running(dev)

static inline int netif_running(const struct net_device *dev)
{
	STOP_HERE("\n");
	return 1;//test_bit(__LINK_STATE_START, &dev->state);
}

static inline int netif_oper_up(const struct net_device *dev)
{
        return 1;//(dev->operstate == IF_OPER_UP ||
                 //dev->operstate == IF_OPER_UNKNOWN /* backward compat */);
}





/* 
 * Note that net_device is renamed to netif above 
 */
static inline struct net_device *
dev_get_by_index(struct net *net, int ifindex)
{
	STOP_HERE("\n");
	return NULL;
}


static inline void *
netdev_priv(const struct net_device *dev)
{
	return (dev->priv_data);
}


static inline int
register_netdevice_notifier(struct notifier_block *nb)
{
    /* LWIP Correlaries
      netif_set_link_callback();
      netif_set_status_callback();
      netif_set_remove_callback();
    */


	return (0);
}

static inline int
unregister_netdevice_notifier(struct notifier_block *nb)
{
	STOP_HERE("\n");
	return (0);
}


struct net_device * ip_dev_find(struct net *net, uint32_t addr);

/** JRL:
 * Do we need to reverse the byte order for the addr's??
 */
static inline int dev_mc_delete(struct net_device *dev, void *addr, int alen, int all)  { return 0; }
static inline int dev_mc_add(struct net_device *dev, void *addr, int alen, int newonly) { return 0; }

#if 0
static inline int
dev_mc_delete(struct net_device *dev, void *addr, int alen, int all)
{
	ip_addr_t mc_addr = {0};
	err_t     err = 0;

	if (alen > sizeof(mc_addr)) {
		printk("Invalid Address Size in %s()\n", __func__);
		return (-EINVAL);
	}

	memset(&mc_addr, 0,    sizeof(mc_addr));
	memcpy(&mc_addr, addr, sizeof(mc_addr));

	err = igmp_leavegroup(&(dev->ip_addr), &mc_addr);

	if (err != ERR_OK) {
		printk("Error leaving Multicast group in %s()\n", __func__);
		return (-EINVAL);
	}
	return 0;
}

static inline int
dev_mc_add(struct net_device *dev, void *addr, int alen, int newonly)
{
	ip_addr_t mc_addr = {0};
	err_t     err = 0;

	if (alen > sizeof(mc_addr)) {
		printk("Invalid Address Size in %s()\n", __func__);
		return (-EINVAL);
	}

	memset(&mc_addr, 0,    sizeof(mc_addr));
	memcpy(&mc_addr, addr, sizeof(mc_addr));

	err = igmp_joingroup(&(dev->ip_addr), &mc_addr);

	if (err != ERR_OK) {
		printk("Error joining Multicast group in %s()\n", __func__);
		return (-EINVAL);
	}
	return 0;
}

#endif
#endif	/* _LINUX_NETDEVICE_H_ */
