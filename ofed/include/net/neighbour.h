#ifndef __NET_NEIGHBOUR_H__
#define __NET_NEIGHBOUR_H__

#include <linux/netdevice.h>
#include <linux/kernel.h>

#define NUD_INCOMPLETE  0x01
#define NUD_REACHABLE   0x02
#define NUD_STALE       0x04
#define NUD_DELAY       0x08
#define NUD_PROBE       0x10
#define NUD_FAILED      0x20

/* Dummy states */
#define NUD_NOARP       0x40
#define NUD_PERMANENT   0x80
#define NUD_NONE        0x00

#define NUD_VALID       (NUD_PERMANENT|NUD_NOARP|NUD_REACHABLE|NUD_PROBE|NUD_STALE|NUD_DELAY)
#define NUD_CONNECTED   (NUD_PERMANENT|NUD_NOARP|NUD_REACHABLE)


struct neighbour
{
        __u8    nud_state;
        unsigned char       ha[ALIGN(MAX_ADDR_LEN, sizeof(unsigned long))];
        struct net_device *dev;
};

struct neigh_table {
};

extern struct neighbour *   neigh_lookup(struct neigh_table *tbl,
                        const void *pkey, struct net_device *dev);

extern void neigh_release(struct neighbour *neigh);

struct sk_buff;
extern int neigh_event_send(struct neighbour *neigh, struct sk_buff *skb);


#endif
