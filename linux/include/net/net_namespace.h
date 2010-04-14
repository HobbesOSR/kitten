#ifndef _NET_NET_NAMESPACE_H
#define _NET_NET_NAMESPACE_H

#include <net/flow.h>
#include <linux/list.h>
struct net {
	struct list_head        dev_base_head;

};
static inline
int net_eq(const struct net *net1, const struct net *net2)
{
	return net1 == net2;
}


#endif
