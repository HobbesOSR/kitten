#include <sys/types.h>
#include <asm/byteorder.h>
#include <lwk/liblwk.h>
#include <infiniband/verbs.h>
#include <unistd.h>

static int ifconfig( __u32 ip_addr, __u32 netmask, union ibv_gid* gid );
static int wait_active( struct ibv_context* ctx, int num_ports, int seconds,
				int* port, int* lid );

__u32 __ip_addr; 
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
			printf("vendor_id=%#x vendor_part_id=%#x hw_ver=%#x"
			" fw_ver=`%s`\n",
				attr.vendor_id, attr.vendor_part_id,
				attr.hw_ver, attr.fw_ver );
			printf("dev=%s guid=%#llx Ports=%d\n", dev_name,
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

				__ip_addr = 0x0a000000 + lid; 	
				__u32 netmask = 0xffffff00;

				char buf[16];
				sprintf(buf,"node-%d",lid);
				sethostname(buf,strlen(buf));
				ifconfig( __ip_addr, netmask, &gid);
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
