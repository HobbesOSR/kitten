
#ifndef _util_h
#define _util_h

#include <stdlib.h>
#include <stdio.h>

#define dbgFail(name, fmt, args...) \
printf("%s::%s():%i:FAILED: " fmt, #name, __FUNCTION__, __LINE__, ## args)

#define terminal(name, fmt, args...) \
{\
printf("%s::%s():%i:FAILED: " fmt, #name, __FUNCTION__, __LINE__, ## args);\
exit(-1); \
}

#if 0
#define dbg(name, fmt, args...) \
printf("%s::%s():%i: " fmt, #name, __FUNCTION__, __LINE__, ## args)
#else
#define dbg(name, fmt, args...) 
#endif

#include <rdma/rdma_cma.h>

static inline uint16_t get_local_lid( int port )
{
    struct ibv_port_attr attr;

    int num;
    struct ibv_context** tmp = rdma_get_devices( &num );

    if ( num != 1 ) {
        terminal( , "what context should we use?\n");
    }

    if (ibv_query_port(tmp[0], port, &attr)) {
        rdma_free_devices( tmp);
        return 0;
    }

    rdma_free_devices( tmp);

    return attr.lid;
}

#endif
