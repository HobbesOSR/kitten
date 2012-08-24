
#ifndef _rdma_procId_h
#define _rdma_procId_h

#include <sys/types.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <types.h>

namespace rdmaPlus {

static inline ProcId getMyProcID()
{
    ProcId id;
    id.pid = getpid();

    struct ifaddrs * ifaddrstruct = NULL;
    struct ifaddrs * ifa = NULL;

    getifaddrs( &ifaddrstruct );

    for ( ifa = ifaddrstruct; ifa != NULL; ifa = ifa->ifa_next) {
        if ( ifa->ifa_addr->sa_family == AF_INET &&
             !strncmp( "ib0", ifa->ifa_name, 3) ) {
            break;
        }
    }
    if ( ifa == NULL ) {
        terminal( ,"getaddrinfo()\n");
    }

    id.nid = ntohl( ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr );

    freeifaddrs( ifaddrstruct );

    return id; 
}

} // namespace rdmaPlus

#endif
