#ifndef _LINUX_IN_H
#define _LINUX_IN_H

#include <linux/types.h>
#include <linux/socket.h>
#include <asm/byteorder.h>

enum {
  IPPROTO_IP = 0,       /* Dummy protocol for TCP       */
  IPPROTO_ICMP = 1,     /* Internet Control Message Protocol    */
  IPPROTO_IGMP = 2,     /* Internet Group Management Protocol   */
  IPPROTO_IPIP = 4,     /* IPIP tunnels (older KA9Q tunnels use 94) */
  IPPROTO_TCP = 6,      /* Transmission Control Protocol    */
  IPPROTO_EGP = 8,      /* Exterior Gateway Protocol        */
  IPPROTO_PUP = 12,     /* PUP protocol             */
  IPPROTO_UDP = 17,     /* User Datagram Protocol       */
  IPPROTO_IDP = 22,     /* XNS IDP protocol         */
  IPPROTO_DCCP = 33,        /* Datagram Congestion Control Protocol */
  IPPROTO_RSVP = 46,        /* RSVP protocol            */
  IPPROTO_GRE = 47,     /* Cisco GRE tunnels (rfc 1701,1702)    */

  IPPROTO_IPV6   = 41,      /* IPv6-in-IPv4 tunnelling      */

  IPPROTO_ESP = 50,            /* Encapsulation Security Payload protocol */
  IPPROTO_AH = 51,             /* Authentication Header protocol       */
  IPPROTO_PIM    = 103,     /* Protocol Independent Multicast   */

  IPPROTO_COMP   = 108,                /* Compression Header protocol */
  IPPROTO_SCTP   = 132,     /* Stream Control Transport Protocol    */

  IPPROTO_RAW    = 255,     /* Raw IP packets           */
  IPPROTO_MAX
};


static inline bool ipv4_is_loopback(__be32 addr)
{
	return (addr & htonl(0xff000000)) == htonl(0x7f000000);
}

static inline bool ipv4_is_zeronet(__be32 addr)
{
	return (addr & htonl(0xff000000)) == htonl(0x00000000);
}

#endif
