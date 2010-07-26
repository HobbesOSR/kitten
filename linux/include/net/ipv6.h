#include <net/sock.h>

#include <net/if_inet6.h>

static inline int ipv6_addr_loopback(const struct in6_addr *a)
{
    LINUX_DBG(TRUE,"\n");

    return 0;
}

static inline void ipv6_addr_copy(struct in6_addr *a1, const struct in6_addr *a2)
{
    LINUX_DBG(TRUE,"\n");
}
