#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <mpi.h>
#include <sys/stat.h>

static void init_ompi(void);

int main(int argc, char *argv[], char *envp[] )
{ 
    int ret;
    int myrank;
    int size;
    //setenv( "OMPI_MCA_orte_debug", "5", 1 );
    //setenv( "OMPI_MCA_btl_base_debug", "5", 1 );
    setenv( "OPAL_PKGDATADIR", "/etc/opal", 1 );
    setenv( "OPAL_SYSCONFDIR", "/etc/opal", 1 );

    printf("calling MPI_Init\n");
    MPI_Init( &argc, & argv);
    printf("MPI_Init done\n");

    MPI_Comm_size( MPI_COMM_WORLD, &size );
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    printf("app size %d myrank %d\n", size, myrank);

    printf("before barrier\n");
    MPI_Barrier( MPI_COMM_WORLD ); 
    printf("after barrier\n");

    printf("calling MPI_Finalize\n");
    MPI_Finalize();
    printf("exiting\n");

    _exit(0);

    return 0;
}
