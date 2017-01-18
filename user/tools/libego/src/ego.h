#ifndef _EGO_H
#define _EGO_H

#include "workloads.h"
#include "ftq.h"
#include "fwq.h"
#include "stencil.h"
#include "selfish.h"
#include <mpi.h>

#define DONE_TAG 0x1a

typedef union {
        selfish_params_t selfish;
        stencil_params_t stencil;
        ftq_params_t     ftq;
        fwq_params_t     fwq;
} params_union_t;

extern MPI_Comm split_comm;
extern MPI_Comm socket_comm;
extern MPI_Comm ego_comm;

extern int _libego_verbose;

int start_task( int *, char *** );
int do_libego_setup( int *, char *** );
int is_ego_process( MPI_Comm );

#endif /* _EGO_H */
