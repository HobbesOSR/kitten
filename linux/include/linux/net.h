#ifndef _LINUX_NET_H
#define _LINUX_NET_H

enum sock_type {
    SOCK_STREAM = 1,
    SOCK_DGRAM  = 2,
    SOCK_RAW    = 3,
    SOCK_RDM    = 4,
    SOCK_SEQPACKET  = 5,
    SOCK_DCCP   = 6,
    SOCK_PACKET = 10,
};

struct socket {
    const struct proto_ops  *ops;
};

struct proto_ops {
    int     (*bind)      (struct socket *sock,
                      struct sockaddr *myaddr,
                      int sockaddr_len);
    int     (*getname)   (struct socket *sock,
                      struct sockaddr *addr,
                      int *sockaddr_len, int peer);
};

static inline void sock_release(struct socket *sock)
{
}

static inline int sock_create_kern(int family, int type, int proto,
                      struct socket **res)
{
    return 0;
}

#endif
