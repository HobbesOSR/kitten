#ifndef _lwipopts_h_
#define _lwipopts_h_

#define SYS_LIGHTWEIGHT_PROT 1
#define NO_SYS 0

#ifdef CONFIG_LWIP_SOCKET
#define LWIP_SOCKET 1
#define LWIP_NETCONN 1
#define LWIP_COMPAT_MUTEX 1
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

// We have struct timeval
#define LWIP_TIMEVAL_PRIVATE 0

#define LWIP_DEBUG 1
#define LWIP_DBG_MIN_LEVEL 1
#define IP_DEBUG LWIP_DBG_OFF
#define TCP_DEBUG LWIP_DBG_ON
#define TCPIP_DEBUG LWIP_DBG_OFF
#define TCP_INPUT_DEBUG LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG LWIP_DBG_ON
#define SOCKETS_DEBUG LWIP_DBG_ON
#define API_LIB_DEBUG LWIP_DBG_ON
#define ICMP_DEBUG LWIP_DBG_ON
#define INET_DEBUG LWIP_DBG_OFF
#define NETIF_DEBUG LWIP_DBG_OFF

#endif
