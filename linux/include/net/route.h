#ifndef _NET_ROUTE_H
#define _NET_ROUTE_H

#include <net/dst.h>
#include <net/ip.h>
#include <net/sock.h>
#include <linux/inetdevice.h>
#include <linux/kernel.h>

struct in_device;

struct rtable {
	union
	{
        	struct dst_entry    dst;
	} u;
	struct in_device    *idev;
	__be32              rt_gateway;
	__be32              rt_src;
};

extern int ip_route_output_key(struct net *net, struct rtable **rt,
         struct flowi *flp);
extern void ip_rt_put(struct rtable * rt);

#endif
