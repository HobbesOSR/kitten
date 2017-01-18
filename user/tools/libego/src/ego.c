#include <mpi.h>
#include "checks.h"
#include "ego.h"
#include "env.h"
#include <stdio.h>
#include <libgen.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#define MAX_EXE_NAME 32
#define MAX_FNAME 256

MPI_Comm split_comm;
MPI_Comm socket_comm;
MPI_Comm ego_comm;
static MPI_Request dreq;

static char *_task = SELFISH_TASK;
static int _1ofN = 16;

static char *_exename;

static dlist_t _libego_data;
static params_union_t _libego_union;
static type_t _libego_bench;
static char _libego_fname[ MAX_FNAME ];
int _libego_verbose = 0;
static FILE *_libego_fp;
static int _my_rank;

static double kernel_accum = 0.0;
static double write_accum = 0.0;

static int
setup_ending( void )
{
        int rc, srank;

        MPI_CHECK( rc = PMPI_Comm_rank( split_comm, &srank ) );

        if( srank == 0 ){
                MPI_CHECK( rc = PMPI_Irecv( NULL, 0, MPI_INT, MPI_ANY_SOURCE,
                                            DONE_TAG, ego_comm, &dreq ) );
        }

        return rc;
}

static int
test_if_done( void )
{
       int rc, srank, test = 0, ssize;
 
       MPI_CHECK( rc = PMPI_Comm_rank( split_comm, &srank ) );
       MPI_CHECK( rc = PMPI_Comm_size( split_comm, &ssize ) );
        
       if( srank == 0 ){
               MPI_CHECK( rc = PMPI_Test( &dreq, &test, MPI_STATUS_IGNORE ) )
       }

       /*
        * We need to agree on if we continue as a group
        */
       MPI_CHECK( PMPI_Allreduce( MPI_IN_PLACE, &test, 1, MPI_INT,
                                  MPI_SUM, split_comm ) );
       
       return ( test > 0 );
}

int
start_task( int *argc, char ***argv )
{
        double start;
        int rank;

        assert( _libego_data.run != NULL );

        assert( _libego_data.write != NULL );

        MPI_CHECK( PMPI_Comm_rank( split_comm, &rank ) );

        while( !test_if_done() ){
                start = PMPI_Wtime();
                _libego_data.run( &_libego_data, NULL );
                kernel_accum += PMPI_Wtime() - start;

                start = PMPI_Wtime();
                _libego_data.write( _libego_fp, &_libego_data, NULL );
                write_accum += PMPI_Wtime() - start;
        }

        fclose( _libego_fp );
        
        /*
         * FIXME: Add stats calculation here ...
         */

        /* 
         * FIXME: Print local stats for library here?
         */

        assert( _libego_data.teardown != NULL );

        _libego_data.teardown( &_libego_data );

        assert( _libego_data.summary != NULL );

        if( rank == 0 )
                _libego_data.summary( stdout );

        if( _libego_verbose ){
                fprintf( stderr, "[ %d ] : Ego thread about to finalize\n",
                                 rank );
        }

        MPI_CHECK( MPI_Finalize() );

        exit( 0 );

        return 0;
}

int
print_header( void )
{
        fprintf( stderr, "# %s library version %s\n",
                         "FIXME", "FIXME" );
        fprintf( stderr, "# Task: %s\n", _workloads_bnames[ _libego_bench ] );
        fprintf( stderr, "# 1 of N: %d\n", _1ofN );

        return 0;
}

static int
is_ego_process_1ofN( MPI_Comm this_comm )
{
        int this_size, this_rank;
        char *value;

        _1ofN = ( ( value = getenv( ONE_OF_N ) ) != NULL )?
                strtod( value, NULL ) :
                ONE_OF_N_DEFAULT;

        MPI_CHECK( PMPI_Comm_rank( this_comm, &this_rank ) );
        MPI_CHECK( PMPI_Comm_size( this_comm, &this_size ) );

        /*
         * FIXME: Want to ensure the total number of processes
         *        is divisible by _1ofN -kbf
         */
        assert( ( this_size % _1ofN ) == 0 );

        return ( ( this_rank % _1ofN ) == 0 );

}

int
is_ego_process( MPI_Comm this_comm )
{
        return is_ego_process_1ofN( this_comm );
}

int
do_libego_setup( int *argc, char ***argv ){
        int rc, color, size;
        int split_size, socket_size;
        char *value;

        if( ( *argv[ 0 ] != NULL ) ){
                _exename = basename(  *( argv[ 0 ] ) );
        } else {
                _exename = "Unknown";
        }

        /*
         * Don't touch MPI_COMM_WORLD, you might hurt it ...
         */

        MPI_CHECK( rc = PMPI_Comm_dup( MPI_COMM_WORLD, &ego_comm ) );

        MPI_CHECK( rc = PMPI_Comm_rank( ego_comm, &_my_rank ) );
        MPI_CHECK( rc = PMPI_Comm_size( ego_comm, &size ) );

        color = is_ego_process( ego_comm );

        /* 
         * Split the app and memhog comms 
         */
        MPI_CHECK( rc = PMPI_Comm_split( ego_comm, color, _my_rank,
                                        &split_comm ) );

        /*
         * Create an on socket comm
         */
        MPI_CHECK( rc = PMPI_Comm_split( ego_comm, _my_rank / _1ofN, _my_rank,
                                        &socket_comm ) );

        /*
         * Sanity checks go here to ensure things setup right
         *
         */
        
        MPI_CHECK( rc = PMPI_Comm_size( split_comm, &split_size ) );

        if( _libego_verbose )
                fprintf( stderr, "[ %d ] : split size: %d\n", _my_rank, split_size );

        MPI_CHECK( rc = PMPI_Comm_size( socket_comm, &socket_size ) );

        if( socket_size != _1ofN ){
                PMPI_Abort( ego_comm, -33 );
        }

        if( is_ego_process( ego_comm ) ){

                int rank;

                MPI_CHECK( PMPI_Comm_rank( split_comm, &rank ) );

                set_params( &_libego_union, &_libego_bench, &_libego_data );

                workloads_configure( &_libego_data, _libego_bench );

                snprintf( _libego_fname, MAX_FNAME, "%s-%s-%d.data",
                                _exename,
                                _workloads_bnames[ _libego_bench ],
                                rank );

                _libego_fp = fopen( _libego_fname, "w" );

                if( _libego_fp == NULL ){
                        perror( "Cannot open outpur file" );
                        abort();
                }

                assert( _libego_data.setup != NULL );

                switch( _libego_bench ){
                        case SELFISH:
                                if( _libego_verbose )
                                        fprintf( stderr, "[ %d ] : selfish.setup()\n",
                                                        _my_rank );
                                _libego_data.setup( &_libego_data,
                                                    &( _libego_union.selfish ) );
                                break;
                        case STENCIL:
                                if( _libego_verbose )
                                        fprintf( stderr, "[ %d ] : stencil.setup()\n",
                                                        _my_rank );
                                _libego_data.setup( &_libego_data,
                                                    &( _libego_union.stencil ) );
                                break;
                        case FTQ:
                                if( _libego_verbose )
                                        fprintf( stderr, "[ %d ] : ftq.setup()\n",
                                                        _my_rank );
                                _libego_data.setup( &_libego_data,
                                                    &( _libego_union.ftq ) );
                                break;
                        case FWQ:
                                if( _libego_verbose )
                                        fprintf( stderr, "[ %d ] : fwq.setup()\n",
                                                        _my_rank );
                                _libego_data.setup( &_libego_data,
                                                    &( _libego_union.fwq ) );
                                break;
                        case SIMPLE_NOISE:
                        case UNDEF:
                        case INVALID:
                                PMPI_Abort( MPI_COMM_WORLD, -11 );
                                exit( -1 );
                }

                if( _libego_verbose )
                        fprintf( stderr, "[ %d ] : Done with setup\n", _my_rank );

                setup_ending();

                if( rank == 0 ){
                        print_header();
                }
        }

        return rc;
}
