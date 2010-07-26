#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
//#include <pthread.h>

#define __USE_GNU 1
#include <sched.h>

static void* func(void*ptr)
{
#if 0
    cpu_set_t set;
    if ( sched_getaffinity( 0, sizeof(set), &set ) ) {
        printf("sched_getaffinity() failed %s\n",strerror( errno ) );
    }

    int i;
    for ( i = 0; i<8; i++ ) {
        printf("%d\n",CPU_ISSET(i,&set));
    }
#endif

    sleep(2);
    _exit(-45);
}

int main(int argc, char *argv[], char *envp[] )
{ 
    int i;
    char* env;
    printf("hello exe=`%s` argc=%d\n", argv[0], argc );
    //return 0;
    //_exit(0);


    for ( i = 1; i < argc; i++ ) {
        printf("arg %d \"%s\"\n",i,argv[i]); 
    } 

    if ( ( env = getenv("HOSTNAME") ) ) {
        printf("%s\n", env );
    }

#if 0
	while( *envp ) {
		printf("%s\n",*envp);
		++envp;
	}
#endif

    #define BUF_LEN 100
    char buf[BUF_LEN];
    snprintf(buf,BUF_LEN,"/proc/%d/app-info",getpid()); 
    FILE* fp = fopen( buf, "r" ); 
    if ( !fp ) {
        printf("fopen() failed, %s\n",strerror(errno));
        _exit(-1);
    } 

    int nRanks,myRank;
    fscanf(fp,"%d %d",&nRanks,&myRank);
    printf("nRanks=%d myRank=%d\n",nRanks,myRank);


#if 0
    cpu_set_t set;
    if ( sched_getaffinity( getpid(), sizeof(set), &set ) ) {
        printf("sched_getaffinity() failed %s\n",strerror( errno ) );
    }

    for ( i = 0; i<8; i++ ) {
        printf("%d\n",CPU_ISSET(i,&set));
    }
#endif
    
    _exit(0);

    return 0;
}
