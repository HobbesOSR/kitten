#ifndef _LINUX_IN_H
#define _LINUX_IN_H

#include <linux/types.h>
#include <linux/socket.h>
#include <asm/byteorder.h>

static inline bool ipv4_is_loopback(__be32 addr)
{
	return (addr & htonl(0xff000000)) == htonl(0x7f000000);
}

static inline bool ipv4_is_zeronet(__be32 addr)
{
	return (addr & htonl(0xff000000)) == htonl(0x00000000);
}

#endif
