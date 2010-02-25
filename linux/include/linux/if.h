#ifndef _LINUX_IF_H
#define _LINUX_IF_H

#include <lwk/if.h>

#define IFF_NOARP   0x80        /* no ARP protocol  */
#define IFF_BONDING 0x20        /* bonding master or slave  */
#define IFF_MASTER  0x400       /* master of a load balancer    */

#endif
