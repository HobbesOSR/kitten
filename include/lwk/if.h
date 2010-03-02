#ifndef _LWK_IF_H
#define _LWK_IF_H

#include <lwk/socket.h>
#include <lwk/netdevice.h>

#define IFNAMSIZ 16

struct lwk_ifreq {
        char    ifr_name[IFNAMSIZ];      /* if name, e.g. "en0" */
        struct  sockaddr ifr_addr;
        struct  sockaddr ifr_netmask;
        unsigned char ifr_hwaddr[MAX_ADDR_LEN];
};

#endif
