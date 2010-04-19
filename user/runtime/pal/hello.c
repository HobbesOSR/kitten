#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include <lwk/liblwk.h>
#include <mpi.h>
#include <sys/stat.h>

int main(int argc, char *argv[], char *envp[] )
{ 
    int i;
    printf("hello %s argc=%d\n", argv[0], argc );

    for ( i = 1; i < argc; i++ ) {
        printf("arg %d \"%s\"\n",i,argv[i]); 
    } 
    char* env = getenv("HOSTNAME");
    if ( env )
    printf("%s\n", env );

    mkfifo( "/tmp/fifo",  0777);
    _exit(1);

    setenv( "OPAL_PKGDATADIR", "/etc/opal", 1 );
    setenv( "OPAL_SYSCONFDIR", "/etc/opal", 1 );
    setenv( "OMPI_MCA_orte_debug", "0", 1 );
    setenv( "OMPI_MCA_btl_base_debug", "0", 1 );

    MPI_Init( &argc, & argv);
    char* buf[100];
    char* sbuf = "hello world";
    MPI_Request request;
    MPI_Status status;
    int ret;

    memset(buf,0,100);

    
printf("calling MPI_Irecv\n");
    ret=MPI_Irecv( buf, strlen(sbuf), MPI_CHARACTER, 0, 0xbeef, MPI_COMM_WORLD, &request);
    if ( ret != MPI_SUCCESS ) {
        printf("MPI_Irecv failed\n");
    }	

printf("calling MPI_Secv\n");
    ret = MPI_Send( sbuf, strlen(sbuf), MPI_CHARACTER, 0, 0xbeef, MPI_COMM_WORLD );
    if ( ret != MPI_SUCCESS ) {
        printf("MPI_Send failed\n");
    }	

printf("calling MPI_Wait\n");
    ret = MPI_Wait(&request,&status);
    if ( ret != MPI_SUCCESS ) {
        printf("MPI_Wait failed\n");
    }	

printf("got `%s`\n",buf);

    MPI_Finalize();

    _exit(0);
#if 0
    struct NidPid {
        uint nid;
        uint pid;
    };
    
    uint* nRanks = (uint*) aspace_find_region_start("NidPidMap");
    struct NidPid* map = (struct NidPid*)(nRanks+1); 

    printf("nRanks=%p nRanks=%d\n",nRanks,*nRanks);

    for ( i = 0; i < *nRanks; i++ ) {
        printf("rank=%d nid=%d pid=%d\n",i,map[i].nid,map[i].pid);
    }
//    aspace_dump2console( getpid() );
#endif
    return -1;
}
