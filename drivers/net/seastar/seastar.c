/** \file
 * Interface to the datagram protocol for the XT3 Seastar.
 *
 * We assume that the Seastar is already mapped as part of the XT3
 * setup so that all we need to do is push commands to it.
 *
 * The Seastar is mapped to the very highest 2 MB of memory through
 * the hard coded page table level2_seastar_pgt in
 * arch/x86_64/kernel/head.S.
 */

#include <lwk/driver.h>
#include <lwk/signal.h>
#include <lwk/netdev.h>
#include <arch/page.h>
#include <arch/proto.h>
#include <lwip/netif.h>
#include <lwip/inet.h>
#include <lwip/ip.h>
#include "memory.h"
#include "command.h"



/**
 * ip_upper_pending structures are used for tracking the transmit of
 * an IP datagram.  They are not used for RX at all.
 *
 * When the IP stack allocates the structure it will set the
 * status to zero.  When the message either completes or is dropped
 * the status will be set to 1 for success or 2 for dropped.
 *
 * The message field at offset zero corresponds to the message
 * field in the ptlhdr.  If the SSNAL_IP_MSG bit is set the
 * pending will be recognized as an IP message by the rest of
 * the Portals stack.
 *
 * \note The layout of the first two fields must match that of
 * the Seastar and the structure must be 128 bytes in size.
 * Do not change this without adjusting the pad.
 */
struct pending
{
	/** Unused for IP TX.  Must be offset 0. */
	uint32_t		message;

	/** Status of TX.  Must be offset 4.
	 * - 0 == In use / still pending
	 * - 1 == Success
	 * - 2 == Failed
	 */
	volatile uint32_t	status;

	/** Host memory pointer to pbuf used for transmit. */
	struct pbuf *		pbuf;

	/** Host memory pointer to singly linked list of pendings */
	struct pending *	next;		

	/** Unused pad to make the structure 128 bytes long */
	uint8_t			pad[ 128 - 24 ];
} PACKED;

sizecheck_struct( pending, 128 );


#define NUM_SKB		64
#define NUM_PENDINGS	64
struct pending upper_pending[ NUM_PENDINGS ];
struct pending * pending_free_list;
uint8_t skb[ NUM_SKB ][ SEASTAR_MTU ];


/** Push a pending onto the free list */
static inline void
seastar_pending_free(
	struct pending *	pending
)
{
	pending->next = pending_free_list;
	pending_free_list = pending;
}


/** Allocate a pending from the free list */
static inline struct pending *
seastar_pending_alloc( void )
{
	struct pending * next = pending_free_list;
	if( !next )
		panic( "%s: No free pendings?\n", __func__ );

	pending_free_list = next->next;
	next->next = 0;

	return next;
}


/** Replace a pre-allocated receive buffer */
static inline void
seastar_refill_skb(
	unsigned		index
)
{
	seastar_skb[ index ] = htaddr( &skb[ index ] );
}



/** Setup the pending free list and initial skb table */
void
seastar_initialize_pendings( void )
{
	int i;
	for( i=0 ; i < NUM_SKB ; i++ )
		seastar_refill_skb( i );

	pending_free_list = 0;

	for( i=0 ; i < NUM_PENDINGS ; i++ )
	{
		struct pending * pending = &upper_pending[i];

		seastar_pending_free( pending );
		pending->message	= 0;
		pending->status		= 0;
		pending->pbuf		= 0;
	}
}



/** Native IP datagram header
 * Note that the lengths are in quad bytes - 1!
 */
struct sshdr {
	uint16_t		lo_length;
	uint8_t			hi_length;
	uint8_t			hdr_dg_type;
} PACKED;

static struct netif seastar_netif;


/** Retrieve an IP packet that has been deposited into host memory.
 *
 * The Seastar has indicated to us that it has received a packet
 * into the skb at the index.  We extract the length of the
 * packet and create a pbuf to store it.
 *
 * The packet is then passed into the lwip stack.  Since we
 * are copying the payload immediately, we can also immediately
 * replenish the lower skb pointer table.
 *
 * \todo Use PBUF_REF rather than PBUF_POOL to avoid copying
 * packets around.
 */
static void
seastar_ip_rx(
	struct netif * const	netif,
	const unsigned		index
)
{
	uint8_t * data = &skb[ index ][0];
	if( index> NUM_SKB )
	{
		printk( KERN_ERR
			"%s: Invalid skb %d from Seastar!\n",
			__func__,
			index
		);
		return;
	}

	const struct sshdr * const sshdr = (void*) data;
	const size_t qb_length = sshdr->hi_length << 16 | sshdr->lo_length;
	size_t len = (qb_length + 1) * 4;
	data += sizeof(*sshdr);
	len -= sizeof(*sshdr);

	// Get a pbuf using our external payload
	// We tell the lwip library that we can use up to the entire
	// size of the skb so that it can resize for reuse.
	// \todo This is broken?
	//struct pbuf * p = pbuf_alloc( PBUF_RAW, len, PBUF_REF );

	// Allocate a pool-allocated buffer and copy into it
	struct pbuf * p = pbuf_alloc( PBUF_RAW, len, PBUF_POOL );
	if( !p )
	{
		printk( KERN_ERR
			"%s: Unable to allocate pbuf! dropping\n",
			__func__
		);
		return;
	}

	//p->payload = data;
	//p->tot_len = len;

	// Copy the memory into the pbuf payload
	struct pbuf * q = p;
	for( q = p ; q != NULL ; q = q->next )
	{
		memcpy( q->payload, data, q->len );
		data += q->len;
	}

	// Since we have copied the packet, we can immediately
	// replace the lower skb.
	seastar_refill_skb( index );

	// Receive the packet
	if( netif->input( p, netif ) != ERR_OK )
	{
		printk( KERN_ERR
			"%s: Packet receive failed!\n",
			__func__
		);
		pbuf_free( p );
		return;
	}
}


/** Clean up after a transmit has completed.
 *
 * When the Seastar has finished sending the packet with the DMA
 * engines, it will signal through the event queue that this pending
 * is no longer in use.  We push it onto the free list and
 * clean up the pbuf that was used.
 *
 * \todo Figure out why pbuf_free() reports an error!
 */
static void
seastar_ip_tx_end(
	const unsigned		index
)
{
	struct pending * const pending = &upper_pending[ index ];

	if(0)
	printk( "%s: tx done %d status %d\n",
		__func__,
		index,
		pending->status
	);

	seastar_pending_free( pending );

	//pbuf_free( pending->pbuf );
}



/** Handle an interrupt from the Seastar device.
 *
 * Process all events on the eq.
 */
void
seastar_interrupt(
	struct pt_regs *	regs,
	unsigned int		vector
)
{
	//printk( "***** %s: We get signal!\n", __func__ );
	int ev_count = 0;

	while( 1 )
	{
		uint32_t e = seastar_next_event();
		if( !e )
			break;

		const unsigned type	= (e >> 16) & 0xFFFF;
		const unsigned index	= (e >>  0) & 0xFFFF;

		//printk( "%d: event %d pending %d\n", ev_count, type, index );
		ev_count++;

#define PTL_EVENT_IP_TX_END	125
#define PTL_EVENT_IP_RX		126
#define PTL_EVENT_IP_RX_EMPTY	127
#define PTL_EVENT_ERR_RX_HDR_CRC 135

		switch( type )
		{
		case PTL_EVENT_IP_RX:
			seastar_ip_rx( &seastar_netif, index );
			break;
		case PTL_EVENT_IP_TX_END:
			seastar_ip_tx_end( index );
			break;
		case PTL_EVENT_ERR_RX_HDR_CRC:
			panic( "%s: rx crc error!\n", __func__ );
			break;
		default:
			printk( KERN_NOTICE
				"***** %s: unhandled event type %d\n",
				__func__,
				type
			);
			break;
		}

	}

	//printk( "%s: processed %d events\n", __func__, ev_count );
}


/** \name Convert an IP address to and from a Seastar NID.
 *
 * \note This uses Cray's assignment of Seastar node ids to IP.
 * 192.168.X.Y, where X = nid/254 and Y = 1 + nid % 254.
 *
 * @{
 */
static inline struct ip_addr
nid2ip(
	unsigned		nid
)
{
	struct ip_addr ip = { htonl( 0
		| 192 << 24
		| 168 << 16
		| ( nid / 254 ) << 8
		| ( 1 + nid % 254 ) << 0
	) };

	return ip;
}


static inline unsigned
ip2nid(
	const struct ip_addr *	ip
)
{
	unsigned bits = ntohl( ip->addr );
	unsigned X = (bits >>  8) & 0xFF;
	unsigned Y = (bits >>  0) & 0xFF;

	return X * 254 + Y - 1;
}

/** @} */


/** Send a packet */
static err_t
seastar_tx(
	struct netif * const	netif,
	struct pbuf * const	p,
	struct ip_addr * const	ipaddr
)
{
	const unsigned nid = ip2nid( ipaddr );

	if(0)
	printk( "%s: Send %d bytes to %08x nid %d\n",
		__func__,
		p->tot_len,
		htonl( *(uint32_t*) ipaddr ),
		nid
	);

	size_t len = sizeof(struct sshdr) + p->tot_len;
	size_t qblen = (len >> 2) - 1;
	struct sshdr sshdr = {
		.lo_length	= (qblen >>  0) & 0xFFFF,
		.hi_length	= (qblen >> 16) & 0x00FF,
		.hdr_dg_type	= (2 << 5), // Datagram 2, type 0 == ip
	};

	struct pending * const pending = seastar_pending_alloc();

#define SSNAL_IP_MSG    0x40000000
	pending->message	= SSNAL_IP_MSG;
	pending->status		= 0;
	pending->pbuf		= p;

	struct msg {
		struct sshdr	sshdr;
		uint8_t		buf[ SEASTAR_MTU ];
	} PACKED;

	static struct msg msg;
	msg.sshdr = sshdr;

	struct pbuf * q;
	size_t offset = 0;
	for( q = p ; q != NULL ; q = q->next )
	{
		memcpy( msg.buf+offset, q->payload, q->len );
		offset += q->len;
	}

	struct command_ip_tx cmd = {
		.op		= COMMAND_IP_TX,
		.nid		= nid,
		.length		= qblen,
		.address	= htaddr( &msg ),
		.pending_index	= pending - upper_pending,
	};

	seastar_cmd( (struct command*) &cmd, 0 );

	return ERR_OK;
}



static err_t
seastar_net_init(
	struct netif * const	netif
)
{
	printk( "%s: Initializing %p (%p)\n", __func__, netif, &seastar_netif );

	netif->mtu		= SEASTAR_MTU;
	netif->flags		= 0
		| NETIF_FLAG_LINK_UP
		| NETIF_FLAG_UP
		;

	netif->hwaddr_len	= 2;
	netif->hwaddr[0]	= (local_nid >> 0) & 0xFF;
	netif->hwaddr[1]	= (local_nid >> 8) & 0xFF;

	netif->name[0]		= 's';
	netif->name[1]		= 's';

	netif->output		= seastar_tx;

	seastar_initialize_pendings();

	return ERR_OK;
}



/** Bring up the Seastar hardware and IP network.
 */
void
seastar_init( void )
{
	if( seastar_hw_init( upper_pending, NUM_PENDINGS ) < 0 )
		return;

	struct ip_addr ipaddr = nid2ip( local_nid );
	struct ip_addr netmask = { htonl( 0xFFFF0000 ) };
	struct ip_addr gw = { htonl( 0x00000000 ) };

	netif_add(
		&seastar_netif, 
		&ipaddr,
		&netmask,
		&gw,
		0,
		seastar_net_init,
		ip_input
	);
}
	


driver_init( "net", seastar_init );
