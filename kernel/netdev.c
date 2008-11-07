#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/params.h>
#include <lwk/driver.h>
#include <lwk/netdev.h>
#include <net/ip.h>

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

	printk( "%s: ip proto %d len %d src %d.%d.%d.%d\n",
		__func__,
		iphdr->tot_len,
		iphdr->protocol,
		(iphdr->saddr >> 24) & 0xFF,
		(iphdr->saddr >> 16) & 0xFF,
		(iphdr->saddr >>  8) & 0xFF,
		(iphdr->saddr >>  0) & 0xFF
	);

	if( iphdr->tot_len != len )
	{
		printk( KERN_ERR
			"%s: Someone is lying about the size of the packet: "
			"hdr len %d != wire len %d\n",
			iphdr->tot_len,
			len
		);
		return;
	}

	int i;
	char buf[ len * 3 + 2 ];
	size_t offset = 0;
	for( i=0 ; i<len ; i++ )
	{
		offset += snprintf(
			buf+offset,
			sizeof(buf)-offset,
			" %02x",
			data[i]
		);
	}

	printk( "%s\n", buf );
}
