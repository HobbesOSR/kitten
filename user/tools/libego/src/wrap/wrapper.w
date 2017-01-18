#include <mpi.h>
#include "checks.h"
#include "ego.h"

#define SHIFT_COMM( this_comm )                         \
{                                                       \
        int comm_result;                                \
        MPI_CHECK( PMPI_Comm_compare( MPI_COMM_WORLD,   \
                                      this_comm,        \
                                     &comm_result ) );  \
                                                        \
        if( comm_result == MPI_IDENT ){                 \
                this_comm = split_comm;                 \
        }                                               \
}
         
{{ fnall foo MPI_Init MPI_Finalize MPI_Init_thread MPI_Wtime MPI_Wtick }}

        {{ applyToType MPI_Comm SHIFT_COMM }}

        MPI_CHECK( {{ ret_val }} = P{{ foo }}( {{ args }} ) );  

{{ endfnall }}

{{ fn fname MPI_Init MPI_Init_thread }}

        int my_rank, srank;

        MPI_CHECK( {{ ret_val }} = P{{ fname }}( {{ args }} ) );        

        MPI_CHECK( PMPI_Comm_rank( MPI_COMM_WORLD, &my_rank ) );

        MPI_CHECK( PMPI_Comm_set_errhandler( MPI_COMM_WORLD, MPI_ERRORS_RETURN ) );

        if( do_libego_setup( {{ 0 }}, {{ 1 }} ) != MPI_SUCCESS ){
                MPI_Abort( ego_comm, -44 );
        }

        MPI_CHECK( PMPI_Barrier( MPI_COMM_WORLD ) );

        MPI_CHECK( PMPI_Comm_rank( split_comm, &srank ));

        if( is_ego_process( ego_comm ) ){
                if( _libego_verbose ){
                        fprintf( stderr, "[ %d ] : Starting ego task\n",
                                         srank );
                }
                start_task( {{ 0 }}, {{ 1 }} );

        } else {
                /*
                 * FIXME: This verbose variable is not set
                 *        from the environment as the app
                 *        processes never pass through the
                 *        code path where they might be 
                 *        set.  Therefore, this will never
                 *        print -kbf
                 */
                if( _libego_verbose ){
                        fprintf( stderr, "[ %d ] : About to start app task\n",
                                         srank );
                }
        }

{{ endfn }}

/*
 * FIXME: For the app processes, the _libego_verbose
 *        is *always* zero and therefore they will
 *        never print -kbf
 */
{{ fn fname MPI_Finalize }}
        int rank, srank, dest, my_rank;

        MPI_CHECK( PMPI_Comm_rank( MPI_COMM_WORLD, &my_rank ) );
        MPI_CHECK( PMPI_Comm_rank( split_comm, &srank ) );

        if( _libego_verbose ){
                fprintf( stderr, "[ %d ] : About to start finalize\n",
                                 my_rank );
        }

        if( !is_ego_process( ego_comm ) ){
                if( srank == 0 ){
                        if( _libego_verbose ){
                                fprintf( stderr, "[ %d ] : Send the done\n",
                                                my_rank );
                        }
                        MPI_CHECK( PMPI_Send( NULL, 0, MPI_INT, 0,
                                                   DONE_TAG, ego_comm ) );
                        if( _libego_verbose ){
                                fprintf( stderr, "[ %d ] : Done sent\n",
                                                my_rank );
                        }
                }
        }

        MPI_CHECK( PMPI_Comm_free( &socket_comm ) );
        MPI_CHECK( PMPI_Comm_free( &split_comm ) );
        MPI_CHECK( PMPI_Comm_free( &ego_comm ) );

        if( _libego_verbose ){
                fprintf( stderr, "[ %d ] : Call finalize\n",
                                 my_rank );
        }

        MPI_CHECK( {{ ret_val }}  = P{{ fname }}( {{ args }} ) );

        if( _libego_verbose ){
                fprintf( stderr, "[ %d ] : Finalize done\n",
                                 my_rank );
        }

{{ endfn }}
