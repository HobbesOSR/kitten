#include <sys/types.h>
#include <sys/socket.h>
#include <asm/byteorder.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints,
                       struct addrinfo **res)
{
	extern __u32 __ip_addr;       
	struct addrinfo* resPtr =  malloc( sizeof( *resPtr) );
	struct sockaddr_in* addr_in = malloc(sizeof(*addr_in));

	if ( ! strncmp( node, "ib0", 3 ) ) {
    		addr_in->sin_addr.s_addr = __cpu_to_be32( __ip_addr );
	} else {
        	int tmp[4];

        	sscanf( node, "%d.%d.%d.%d", tmp+3, tmp+2, tmp+1, tmp );

        	__be32 addr = ( tmp[3] << 24 ) | ( tmp[2] << 16 ) |
                                                ( tmp[1] << 8 ) | tmp[0];

    		addr_in->sin_addr.s_addr = __cpu_to_be32(addr);
	}
#if 0 
	printf("%s() node=%s %#x\n",__FUNCTION__,node,
			addr_in->sin_addr.s_addr);
#endif
	
	addr_in->sin_port = 0;

	resPtr->ai_flags = 40;
	resPtr->ai_family = AF_INET;
	resPtr->ai_socktype = SOCK_STREAM;
	resPtr->ai_protocol = IPPROTO_TCP;
	resPtr->ai_addrlen = sizeof( *addr_in );
	resPtr->ai_addr = (struct sockaddr*)addr_in;
	resPtr->ai_addr->sa_family = PF_INET;
	resPtr->ai_next = NULL;

	*res = resPtr;

	return 0;
}

void freeaddrinfo(struct addrinfo* res )
{
    //printf("%s %p %p\n",__FUNCTION__,res,res->ai_addr);
    free( res->ai_addr );
    free( res );
}
