#ifndef _lwipopts_h_
#define _lwipopts_h_

#define NO_SYS 1

#ifdef CONFIG_LWIP_SOCKET
#define LWIP_SOCKET 1
#define LWIP_NETCONN 1
#else
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0
#endif


#ifdef CONFIG_LWIP_ARP
#define LWIP_ARP 1
#else
#define LWIP_ARP 0
#define ARP_QUEUEING 0
#endif

#define LWIP_DEBUG 1
#define IP_DEBUG LWIP_DBG_ON
#define ICMP_DEBUG LWIP_DBG_ON
#define INET_DEBUG LWIP_DBG_OFF
#define NETIF_DEBUG LWIP_DBG_OFF

#define LWIP_PLATFORM_DIAG(x) printk x

#endif
