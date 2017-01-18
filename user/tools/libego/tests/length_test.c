#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "checks.h"
#include <assert.h>

int
main( int argc, char **argv )
{
        int rank=-1, size=-1;
        unsigned int left;

        MPI_CHECK( MPI_Init( &argc, &argv ) );
        MPI_CHECK( MPI_Comm_size( MPI_COMM_WORLD, &size ) );
        MPI_CHECK( MPI_Comm_rank( MPI_COMM_WORLD, &rank ) );

        fprintf( stdout, "Hello from rank %d of %d\n", rank, size );

        left = sleep( 10 * 60 );

        if( left != 0 ){
                fprintf( stdout, "Left in Sleep: %ud secs\n",
                                 left );
        }

        MPI_CHECK( MPI_Finalize() );

        return 0;
}

