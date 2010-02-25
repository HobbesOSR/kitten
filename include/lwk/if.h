#ifndef _LWK_IF_H
#define _LWK_IF_H

#include <lwk/socket.h>
#include <lwk/netdevice.h>

#define IFNAMSIZ 16
struct lwk_ifreq {
        union
        {
                char    ifrn_name[IFNAMSIZ];      /* if name, e.g. "en0" */
        } ifr_ifrn;

        union {
                struct  sockaddr ifru_addr;
                struct  sockaddr ifru_netmask;
                unsigned char ifru_hwaddr[MAX_ADDR_LEN];
        } ifr_ifru;

};

#define ifr_name        ifr_ifrn.ifrn_name      /* interface name       */
#define ifr_hwaddr      ifr_ifru.ifru_hwaddr    /* MAC address          */
#define ifr_addr        ifr_ifru.ifru_addr      /* address              */
#define ifr_netmask     ifr_ifru.ifru_netmask   /* interface net mask   */

#endif
