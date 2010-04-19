#ifndef PCT_FS_MSGS_H
#define PCT_FS_MSGS_H

#include <rdma/rdma.h>

struct IOMsg {
    RDMA::key_t  rdma_key;
    RDMA::addr_t rdma_addr;
    size_t len;
    size_t off;
    int    flags;
    int    handle;
};

struct SysCallReq {
    int  syscall;
    uint pid;
    union {
        IOMsg    write;
        IOMsg    read;
    } u;
    RDMA::key_t  resp_key;
};


#endif
