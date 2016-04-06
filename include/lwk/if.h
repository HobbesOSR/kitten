#ifndef _LWK_IF_H
#define _LWK_IF_H

#include <lwk/socket.h>
#include <lwk/netdevice.h>

#define IFNAMSIZ 16
#define IFF_LOOPBACK    0x8

struct lwk_ifreq {
        char    ifr_name[IFNAMSIZ];      /* if name, e.g. "en0" */
        struct  sockaddr ifr_addr;
        struct  sockaddr ifr_netmask;
        unsigned char ifr_hwaddr[MAX_ARP_ADDR_LEN];
};

extern int lwk_ifconfig( struct lwk_ifreq* req );

#endif
