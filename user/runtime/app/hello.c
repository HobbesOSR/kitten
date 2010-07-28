#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#define __USE_GNU 1
#include <sched.h>

int main(int argc, char *argv[], char *envp[] )
{ 
    int i;
    char* env;
    printf("hello exe=`%s` argc=%d\n", argv[0], argc );

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

    _exit(0);

    return 0;
}
