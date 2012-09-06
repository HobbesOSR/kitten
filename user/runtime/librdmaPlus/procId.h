
#ifndef _rdma_procId_h
#define _rdma_procId_h

#ifndef USING_LWK
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#else
#include <netdb.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <types.h>

namespace rdmaPlus {

static inline ProcId getMyProcID()
{
    ProcId id;
    id.pid = getpid();

#ifndef USING_LWK
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
        terminal( ,"getifaddrs()\n");
    }

    id.nid = ntohl( ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr );

    freeifaddrs( ifaddrstruct );
#else
    struct addrinfo *target;

    if ( getaddrinfo( "ib0", NULL, NULL, &target ) != 0 ) {
            terminal( ,"getaddrinfo()\n");
    }

    id.nid = ntohl( ((struct sockaddr_in*)target->ai_addr)->sin_addr.s_addr );

    freeaddrinfo( target );
#endif

    return id; 
}

} // namespace rdmaPlus

#endif
