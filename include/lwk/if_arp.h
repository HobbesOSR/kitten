#ifndef _LWK_IF_ARP_H
#define _LWK_IF_ARP_H

#include <lwk/socket.h>
#include <lwk/netdevice.h>

#define ARPHRD_INFINIBAND 32      /* InfiniBand           */
#define ARPHRD_ETHER       1      /* Ethernet 10Mbps      */

/* ARP Flag values */
#define ATF_PUBL        0x08      /* publish entry        */

struct lwk_arpreq {
	struct sockaddr arp_pa;
	char            arp_ha[MAX_ARP_ADDR_LEN];
	int             arp_flags;
	char            arp_dev[16];
};

extern int lwk_arp(struct lwk_arpreq* req);

#endif
