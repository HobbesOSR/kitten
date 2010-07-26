
#ifndef _rdma_procId_h
#define _rdma_procId_h

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include <types.h>

namespace rdmaPlus {

static inline ProcId getMyProcID()
{
    ProcId id;
    id.pid = getpid();

    struct addrinfo *target;

    if ( getaddrinfo( "ib0", NULL, NULL, &target ) != 0 ) {
            terminal( ,"getaddrinfo()\n");
    }

    id.nid = ntohl( ((struct sockaddr_in*)target->ai_addr)->sin_addr.s_addr );

    freeaddrinfo( target );

    return id; 
}

} // namespace rdmaPlus

#endif
