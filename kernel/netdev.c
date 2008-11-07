#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/params.h>
#include <lwk/driver.h>
#include <lwk/netdev.h>
#include <net/ip.h>
#include <net/icmp.h>

/**
 * List of all registered network devices in the system.
 */
static LIST_HEAD(netdev_list);

/**
 * Holds a comma separated list of network devices to configure.
 */
static char netdev_str[128];
param_string(net, netdev_str, sizeof(netdev_str));

/**
 * Registers a new network device
 */
void
netdev_register(
	struct netdev *		dev
)
{
	list_add( &dev->next, &netdev_list );
}


/**
 * Initializes the network subsystem; called once at boot.
 */
void
netdev_init(void)
{
	printk( KERN_INFO "%s: Bringing up network devices: '%s'\n", __func__, netdev_str );
	driver_init_list( "net", netdev_str );
}


/** Format a data buffer as a series of hex bytes.
 */
const char *
hexdump(
	const void *		data_v,
	size_t			len
)
{
	int i;
	const uint8_t * const data = data_v;
	static char buf[ 32*3 + 5 ];

	size_t offset = 0;
	for( i=0 ; i<len && offset < sizeof(buf)-10 ; i++ )
	{
		offset += snprintf(
			buf+offset,
			sizeof(buf)-offset,
			"%02x ",
			data[i]
		);
	}

	if( i < len )
		snprintf(
			buf + offset,
			sizeof(buf)-offset,
			"..."
		);
	else
		buf[offset] = '\0';

	return buf;
}


/** Format an IP address as a string.
 *
 * \note Not thread safe.  Not for multiple use in one call to printk.
 * \note Address must be in host byte order.
 */
const char *
inet_ntoa(
	uint32_t		addr
)
{
	static char buf[ 4*4 + 1 ];
	snprintf( buf, sizeof(buf),
		"%d.%d.%d.%d",
		(addr >> 24) & 0xFF,
		(addr >> 16) & 0xFF,
		(addr >>  8) & 0xFF,
		(addr >>  0) & 0xFF
	);

	return buf;
}

/** Process an ICMP packet
 */
static void
handle_icmp(
	struct iphdr * const	iphdr,
	uint8_t *		data
)
{
	printk( "%s: icmp from %s len %d\n",
		__func__,
		inet_ntoa( iphdr->saddr ),
		iphdr->tot_len
	);

	struct icmphdr * const icmp = (void*) data;
	data += sizeof(*icmp);

	if( icmp->type != ICMP_ECHO )
	{
		printk( KERN_NOTICE
			"%s: Unhandled icmp type %d\n",
			__func__,
			icmp->type
		);
		return;
	}

	printk( "echo type %d code %d crc %d id %d seq %d\n",
		icmp->type,
		icmp->code,
		ntohs( icmp->checksum ),
		ntohs( icmp->un.echo.id ),
		ntohs( icmp->un.echo.sequence )
	);

	printk( "%s\n", hexdump( data, iphdr->tot_len - sizeof(*icmp) ) );

	struct icmphdr icmp_reply = {
		.type		= htons( ICMP_ECHO ),
		.code		= 0,
		.checksum	= 0,
		.un.echo.id	= icmp->un.echo.id,
		.un.echo.sequence = icmp->un.echo.sequence,
	};

	netdev_tx(
		iphdr->saddr,
		iphdr->daddr,
		IP_PROTO_ICMP,
		(void*) &icmp_reply,
		sizeof(icmp_reply)
	);
}


/** Receive a packet
 */
void
netdev_rx(
	uint8_t *		data,
	size_t			len
)
{
	if( len > NETDEV_MTU )
	{
		printk( KERN_ERR
			"%s: Packet length %ld overflows mtu %ld!\n",
			__func__,
			len,
			NETDEV_MTU
		);

		return;
	}

	struct iphdr * const iphdr = (void*) data;
	data += sizeof(*iphdr);
	len -= sizeof(*iphdr);

	// Convert the ip header to host byteorder
	iphdr->tot_len	= ntohs( iphdr->tot_len ) - sizeof(*iphdr);
	iphdr->id	= ntohs( iphdr->id );
	iphdr->frag_off	= ntohs( iphdr->frag_off );
	iphdr->check	= ntohs( iphdr->check );
	iphdr->saddr	= ntohl( iphdr->saddr );
	iphdr->daddr	= ntohl( iphdr->daddr );

	printk( "%s: ip proto %d len %d src %s\n",
		__func__,
		iphdr->protocol,
		iphdr->tot_len,
		inet_ntoa( iphdr->saddr )
	);

	if( iphdr->tot_len != len )
	{
		printk( KERN_ERR
			"%s: Someone is lying about the size of the packet: "
			"hdr len %d != wire len %ld\n",
			__func__,
			iphdr->tot_len,
			len
		);
		return;
	}

	switch( iphdr->protocol )
	{
	case IP_PROTO_ICMP:
		handle_icmp( iphdr, data );
		break;
	default:
		printk( KERN_NOTICE
			"%s: Unhandled protocol %d from %s:%s\n",
			__func__,
			iphdr->protocol,
			inet_ntoa( iphdr->saddr ),
			hexdump( data, len )
		);
		break;
	}

}


void
netdev_tx(
	uint32_t		dest,
	uint32_t		src,
	uint8_t			protocol,
	const void *		data,
	size_t			len
)
{
	struct iphdr hdr = {
		.version	= 4,
		.tot_len	= htons( sizeof(struct iphdr) + len ),
		.ttl		= 64,
		.protocol	= protocol,
		.saddr		= htonl( src ),
		.daddr		= htonl( dest )
	};

	seastar_tx( &hdr, data, len );
}
