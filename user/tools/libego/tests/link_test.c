#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include "checks.h"
#include <assert.h>

int
main( int argc, char **argv )
{
        int rank=-1, size=-1;

        MPI_CHECK( MPI_Init( &argc, &argv ) );
        MPI_CHECK( MPI_Comm_size( MPI_COMM_WORLD, &size ) );
        MPI_CHECK( MPI_Comm_rank( MPI_COMM_WORLD, &rank ) );

        fprintf( stdout, "Hello from rank %d of %d\n", rank, size );

        MPI_CHECK( MPI_Finalize() );

        return 0;
}

