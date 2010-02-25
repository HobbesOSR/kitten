#ifndef _LWK_SOCKET_H
#define _LWK_SOCKET_H

#include <linux/types.h>

typedef unsigned short  sa_family_t;

struct sockaddr {
    sa_family_t sa_family;  /* address family, AF_xxx   */
    char        sa_data[14];    /* 14 bytes of protocol address */
};

struct in_addr {
    __be32 s_addr;
};

struct sockaddr_in {
  sa_family_t       sin_family; /* Address family       */
  __be16        sin_port;   /* Port number          */
  struct in_addr    sin_addr;   /* Internet address     */
};

#endif
