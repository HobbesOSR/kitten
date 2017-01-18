#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "ego.h"
#include "env.h"
#include "workloads.h"

int
set_params( params_union_t *u, type_t *b, dlist_t *d )
{
        char *value;

        assert( u != NULL );
        assert( b != NULL );
        assert( d != NULL );

        value = getenv( EGO_TASK );

        if( value == NULL ){
                value = SELFISH_TASK;
        }

        if( strcmp( STENCIL_TASK, value ) == 0 ){
                *b = STENCIL;
        } else if( strcmp( FTQ_TASK, value ) == 0 ){
                *b = FTQ;
        } else if( strcmp( FWQ_TASK, value ) == 0 ){
                *b = FWQ;
        } else {
                *b = SELFISH;
        }

        _libego_verbose = d->verbose = 
                ( ( value = getenv( EGO_VERBOSE ) ) != NULL )?
                     strtol( value, NULL, 10 ) : 
                     EGO_VERBOSE_DEFAULT;

        d->duration = ( ( value = getenv( EGO_TASK_LENGTH ) ) != NULL )?
                      strtol( value, NULL, 10 ) : EGO_TASK_LENGTH_DEFAULT;

        switch( *b ){
                case SELFISH:
                        u->selfish.threshold = 
                                ( ( value = getenv( EGO_SELFISH_THRESHOLD ) ) != NULL )?
                                strtoll( value, NULL, 10 ) :
                                EGO_SELFISH_THRESHOLD_DEFAULT;
                        u->selfish.size =
                                ( ( value = getenv( EGO_SELFISH_ASIZE ) ) != NULL )?
                                strtol( value, NULL, 10 ) :
                                EGO_SELFISH_ASIZE_DEFAULT;
                        u->selfish.comm = split_comm;
                        break;
                case STENCIL:
                        u->stencil.ndims =
                                ( ( value = getenv( EGO_STENCIL_NDIMS ) ) != NULL )?
                                strtol( value, NULL, 10 ) :
                                EGO_STENCIL_NDIMS_DEFAULT;
                        u->stencil.npx =
                                ( ( value = getenv( EGO_STENCIL_NPX ) ) != NULL )?
                                strtol( value, NULL, 10 ) :
                                EGO_STENCIL_NPX_DEFAULT;
                        u->stencil.npy =
                                ( ( value = getenv( EGO_STENCIL_NPY ) ) != NULL )?
                                strtol( value, NULL, 10 ) :
                                EGO_STENCIL_NPY_DEFAULT;
                        u->stencil.npz =
                                ( ( value = getenv( EGO_STENCIL_NPZ ) ) != NULL )?
                                strtol( value, NULL, 10 ) :
                                EGO_STENCIL_NPZ_DEFAULT;
                        u->stencil.usecs =
                                ( ( value = getenv( EGO_STENCIL_DURATION ) ) != NULL )?
                                strtol( value, NULL, 10 ) :
                                EGO_STENCIL_DURATION_DEFAULT;
                        u->stencil.comm = split_comm;
                        break;
                case FWQ:
                        u->fwq.usecs =
                                ( ( value = getenv( EGO_FWQ_DURATION ) ) != NULL )?
                                strtol( value, NULL, 10 ) :
                                EGO_FWQ_DURATION_DEFAULT;
                        u->fwq.comm = split_comm;
                        break;
                case FTQ:
                        u->ftq.usecs =
                                ( ( value = getenv( EGO_FTQ_DURATION ) ) != NULL )?
                                strtol( value, NULL, 10 ) :
                                EGO_FTQ_DURATION_DEFAULT;
                        u->ftq.comm = split_comm;
                        break;
                case SIMPLE_NOISE:
                case UNDEF:
                case INVALID:
                        PMPI_Abort( MPI_COMM_WORLD, -13 );
                        exit( -1 );
        }

        return 0;
}
