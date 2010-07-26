

#include <msg.h>
#include <stdio.h>
#include <stdlib.h>
#include <rdmaPlus.h>


using namespace rdmaPlus;
Rdma foo;

#define SIZE 10

static void client( int port )
{
    printf("client: server port=%d\n",port);

    ProcId id = { 0xa0000fe, port };    
    
    foo.connect( id );

    Request request;

    char buf[SIZE];
    
    memcpy(buf,"hello", sizeof( "hello" ) );
    foo.isend( buf, SIZE, id, 0xdead, request );

    Status status;

    foo.wait( request, -1, &status );

    foo.disconnect( id );
}

static void server( )
{
    printf("server: listen on port %d\n", getpid() );

    Request request;

    char buf[SIZE];

    ProcId id = { -1, -1 };

while( 1) {
    foo.irecv( buf, SIZE, id, 0xdead, request );

    Status status;

    foo.wait( request, -1, &status );

    printf("id=[%#x,%d] tag=%#x\n",status.key.id.nid, status.key.id.pid, status.key.tag );
    printf("got `%s`\n",buf);
}
}

int main(int argc, char **argv)
{

    if ( argc == 2) {
        client( atoi(argv[1]) );    
    } else {
        server();    
    } 

    return 0;
}
