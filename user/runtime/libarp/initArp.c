#include <sys/types.h>
#include <asm/byteorder.h>
#include <lwk/liblwk.h>

static int arp( char* ip, char *hw )
{
	struct lwk_arpreq req;
 	int tmp[4];

//	printf("arp() ip=%s hw=%s\n",ip,hw);
	sscanf( ip, "%d.%d.%d.%d", tmp+3, tmp+2, tmp+1, tmp );

	__be32 addr = ( tmp[3] << 24 ) | ( tmp[2] << 16 ) |
						( tmp[1] << 8 ) | tmp[0];

	strcpy( req.arp_dev, "ib0" ); 
	req.arp_flags = ATF_PUBL;

	((struct sockaddr_in*)&req.arp_pa)->sin_addr.s_addr =
							__be32_to_cpu(addr);

	int i;
	for ( i = 0; i < MAX_ADDR_LEN && strlen(hw); i++ ) {
		int tmp;
		sscanf( hw, "%x:", &tmp );
		req.arp_ha[i] = tmp;
		while( *hw != 0 && *hw != ':' ) ++hw;
		if ( *hw == ':' ) ++hw;
	}
	lwk_arp(&req);
	return 0;
}

static __attribute__ ((constructor (65535))) void init(void)
{
arp( "10.0.0.1","00:00:00:00:00:23:7d:ff:ff:95:75:55" ); 
arp( "10.0.0.2","00:00:00:00:00:23:7d:ff:ff:95:75:65" );
arp( "10.0.0.3","00:00:00:00:00:23:7d:ff:ff:95:26:f9" );
arp( "10.0.0.4","00:00:00:00:00:23:7d:ff:ff:95:16:45" );
arp( "10.0.0.5","00:00:00:00:00:23:7d:ff:ff:95:15:e9" );
arp( "10.0.0.6","00:00:00:00:00:23:7d:ff:ff:95:16:2d" );
arp( "10.0.0.7","00:00:00:00:00:23:7d:ff:ff:95:26:31" );
arp( "10.0.0.8","00:00:00:00:00:23:7d:ff:ff:95:16:29" );
arp( "10.0.0.9","00:00:00:00:00:23:7d:ff:ff:95:75:61" );
arp( "10.0.0.10","00:00:00:00:00:23:7d:ff:ff:95:75:51" );
arp( "10.0.0.11","00:00:00:00:00:23:7d:ff:ff:95:75:bd" );
arp( "10.0.0.12","00:00:00:00:00:23:7d:ff:ff:95:15:f5" );
arp( "10.0.0.13","00:00:00:00:00:23:7d:ff:ff:95:75:d5" );
arp( "10.0.0.14","00:00:00:00:00:23:7d:ff:ff:95:75:c5" );
arp( "10.0.0.15","00:00:00:00:00:23:7d:ff:ff:95:75:c9" );
arp( "10.0.0.16","00:00:00:00:00:23:7d:ff:ff:95:15:f1" );

}
