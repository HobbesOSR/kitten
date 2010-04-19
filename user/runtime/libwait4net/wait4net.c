#include <sys/types.h>
#include <asm/byteorder.h>
#include <lwk/liblwk.h>
#include <infiniband/verbs.h>
#include <unistd.h>

static int ifconfig( __u32 ip_addr, __u32 netmask, union ibv_gid* gid );
static int wait_active( struct ibv_context* ctx, int num_ports, int seconds,
				int* port, int* lid );

static __u32 ip_addr; 
static __attribute__ ((constructor (65534))) void init(void)
{
	struct ibv_device** list = NULL;
	int num;
	list = ibv_get_device_list( &num );

	if ( !list ) {
		printf("couldn't find any InfiniBand devices!\n");
		while(1);
	}

	int i;
	for ( i = 0; list[i]; ++i ) {
		const char *dev_name = ibv_get_device_name( list[i] );

		struct ibv_context* ctx = ibv_open_device( list[i] );
		if ( ! ctx ) {
			printf("couldn't open device %s!\n", dev_name );
			continue;
		}

		struct ibv_device_attr attr;
		if ( ibv_query_device( ctx, &attr ) ) {
			printf("query device %s failed!\n", dev_name );
		} else {
			printf("dev=%s guid=%#llx ports=%d\n", dev_name,
			__be64_to_cpu( attr.node_guid ), attr.phys_port_cnt );
			
			int port,lid;
			if ( wait_active( ctx, attr.phys_port_cnt,
							1000, &port, &lid ) ) 
			{
				union ibv_gid gid;
				if ( ibv_query_gid( ctx, port, 0, &gid ) ) {
					printf("ibv_query_gid failed!\n");
					while(1);
				}
				printf("found, port=%d lid=%d %#llx %#llx\n",
					port,lid,
       					__be64_to_cpu(gid.global.subnet_prefix),
        				__be64_to_cpu(gid.global.interface_id));

				ip_addr = 0x0a000000 + lid; 	
				__u32 netmask = 0xffffff00;

				char buf[16];
				sprintf(buf,"node-%d",lid);
				sethostname(buf,strlen(buf));
				ifconfig( ip_addr, netmask, &gid);
				ibv_close_device( ctx); 
				ibv_free_device_list( list );
				return; 
			}
		}
		
		ibv_close_device( ctx); 
	}

	ibv_free_device_list( list );
}

static int wait_active( struct ibv_context* ctx, int num_ports, int seconds,
				int* port, int* lid )
{
	struct ibv_port_attr port_attr;
	int i;
	int count = 0;
	printf("looking for active port\n");
	while( count < seconds ) {
		for ( i = 1; i <= num_ports;  i++ ) {
			ibv_query_port( ctx, i, &port_attr );
			if ( port_attr.state == IBV_PORT_ACTIVE ) {
				*port = i; 
				*lid = port_attr.lid;
				return 1;
			}	
		}
		sleep(1);
		++count;
	}
	return 0;
}

static int ifconfig( __u32 ip_addr, __u32 netmask, union ibv_gid* gid )
{
	struct lwk_ifreq req;
    	((struct sockaddr_in*)&req.ifr_addr)->sin_addr.s_addr = 
						__cpu_to_be32(ip_addr);
    	((struct sockaddr_in*)&req.ifr_netmask)->sin_addr.s_addr = 
						__cpu_to_be32(netmask);
	strcpy( req.ifr_name, "ib0" ); 
	memset( req.ifr_hwaddr, 0, MAX_ADDR_LEN );
	memcpy( req.ifr_hwaddr + 4, gid->raw, 16 );
	return lwk_ifconfig(&req);
}

#include <stdlib.h>

      struct addrinfo {
           int     ai_flags;
           int     ai_family;
           int     ai_socktype;
           int     ai_protocol;
           size_t  ai_addrlen;
           struct sockaddr *ai_addr;
           char   *ai_canonname;
           struct addrinfo *ai_next;
       };


int getaddrinfo(const char *node, const char *service,
                       const struct addrinfo *hints,
                       struct addrinfo **res)
{
	struct addrinfo* resPtr=  malloc( sizeof( *resPtr) );
	struct sockaddr_in* addr_in = malloc(sizeof(*addr_in));


	if ( ! strncmp( node, "ib0", 3 ) ) {
    		addr_in->sin_addr.s_addr = __cpu_to_be32(ip_addr);
	} else {
        	int tmp[4];

        	sscanf( node, "%d.%d.%d.%d", tmp+3, tmp+2, tmp+1, tmp );

        	__be32 addr = ( tmp[3] << 24 ) | ( tmp[2] << 16 ) |
                                                ( tmp[1] << 8 ) | tmp[0];

    		addr_in->sin_addr.s_addr = __cpu_to_be32(addr);
	}
#if 1 
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
