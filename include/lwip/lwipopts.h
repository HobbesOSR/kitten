#ifndef _lwipopts_h_
#define _lwipopts_h_

#define SYS_LIGHTWEIGHT_PROT 1
#define NO_SYS 0
#define MEM_LIBC_MALLOC 1
#define MEMP_MEM_MALLOC 1
#define LWIP_SO_SNDBUF 1
#define LWIP_SO_RCVBUF 1

// BJK: These options are needed to support larger UDP messages. Tune as needed.
// Note: Need invariant PBUF_POOL_SIZE > IP_REASS_MAX_PBUFS
// Maximum outstanding messages in the mailbox. To be safe, message sizes should be
// less than MTU * TCPIP_MBOX_SIZE (other processes can consume slots as well)
#define TCPIP_MBOX_SIZE 32

// Number of pbufs in the pool
#define PBUF_POOL_SIZE 64

// Number of pbufs that can be used in a single packet reassembly. Message size,
// then, is limited to MTU * IP_REASS_MAX_PBUFS
#define IP_REASS_MAX_PBUFS 32

#ifdef CONFIG_LWIP_SOCKET
#define LWIP_SOCKET 1
#define LWIP_NETCONN 1
#define LWIP_COMPAT_MUTEX 1
#else
#define LWIP_SOCKET 0
#define LWIP_NETCONN 0
#define LWIP_COMPAT_MUTEX 1
#endif

#ifdef CONFIG_LWIP_ARP
#define LWIP_ARP 1
#define ARP_QUEUEING 1
#else
#define LWIP_ARP 0
#define ARP_QUEUEING 0
#endif

#ifdef CONFIG_LWIP_DHCP
#define LWIP_DHCP 1
#define LWIP_DHCP_BOOTP_FILE 0
#else
#define LWIP_DHCP 0
#endif

// We have struct timeval
//#define LWIP_TIMEVAL_PRIVATE 0

#define LWIP_DEBUG 1
#define IP_REASS_DEBUG LWIP_DBG_OFF
#define LWIP_DBG_MIN_LEVEL LWIP_DBG_LEVEL_ALL
#define IP_DEBUG LWIP_DBG_OFF
#define UDP_DEBUG LWIP_DBG_OFF
#define TCP_DEBUG LWIP_DBG_OFF
#define TCPIP_DEBUG LWIP_DBG_OFF
#define TCP_INPUT_DEBUG LWIP_DBG_OFF
#define TCP_OUTPUT_DEBUG LWIP_DBG_OFF
#define SOCKETS_DEBUG LWIP_DBG_OFF
#define API_LIB_DEBUG LWIP_DBG_OFF
#define ICMP_DEBUG LWIP_DBG_OFF
#define INET_DEBUG LWIP_DBG_OFF
#define NETIF_DEBUG LWIP_DBG_OFF

#endif
