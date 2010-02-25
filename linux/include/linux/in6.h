#ifndef _LINUX_IN6_H
#define _LINUX_IN6_H

#include <linux/types.h>

struct in6_addr
{
	union
	{
		__u8        u6_addr8[16];
		__be16      u6_addr16[8];
		__be32      u6_addr32[4];
	} in6_u;
#define s6_addr         in6_u.u6_addr8
#define s6_addr16       in6_u.u6_addr16
#define s6_addr32       in6_u.u6_addr32
};

struct sockaddr_in6 {
	unsigned short int  sin6_family;/* AF_INET6 */
	__be16          sin6_port;      /* Transport layer port # */
	__be32          sin6_flowinfo;  /* IPv6 flow information */
	struct in6_addr     sin6_addr;  /* IPv6 address */
	__u32           sin6_scope_id;  /* scope id (new in RFC2553) */
};

#endif
