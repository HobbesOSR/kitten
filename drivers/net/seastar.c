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
#include <net/seastar.h>
#include <lwip/netif.h>
#include <lwip/inet.h>
#include <lwip/ip.h>

#define SEASTAR_MTU		8192

#define HTB_MAP			0x000020000
#define HTB_VALID_FLAG		0x8000

#define seastar_tx_source	0xFFE00108
#define seastar_phys_base	0xFFFA0000
#define seastar_mailbox_base	0xFFFA0000
#define seastar_skb_base	0xFFFA4000
#define seastar_host_base	0xFFFA5000
#define seastar_htb_base	0xFFE20000
#define seastar_niccb_base	0xFFFFE000

/** The Seastar is mapped at L4 entry 511, L3 entry 511, L2 entry 511,
 * giving us a 2 MB window at the very highest end of memory into the
 * Seastar and its memory mapped devices.
 */
#define seastar_virt_base ( 0xFFFFFFFFull << 32 )
//((uint8_t*) ~( (1<<20)-1 ))
//(511ull << 36) | (511ull << 29) | (511ull <<20) )

/** Location in host memory of PPC's nic control block structure */
static struct niccb * const niccb
	= (void*)( seastar_virt_base + seastar_niccb_base );

/** 64-bit HT quad-word address of incoming datagram buffers */
static uint64_t * const seastar_skb
	= (void*)( seastar_virt_base + seastar_skb_base );

/** Location in host memory of PPC's hypertransport map */
static volatile uint32_t * const htb_map
	= (void*)( seastar_virt_base + seastar_htb_base );

/** Start of host memory buffer mapped into Seastar memory through HTB */
paddr_t seastar_host_region_phys;




#ifndef PACKED
#define PACKED	__attribute__((packed))
#endif
#ifndef ALIGNED
#define ALIGNED __attribute__((aligned))
#endif
#ifndef PAGE_ALIGNED
#define PAGE_ALIGNED __attribute__((aligned(4096)))
#endif


#define GENERIC_PROCESS_INDEX	1
#define COMMAND_Q_LENGTH	63
#define RESULT_Q_LENGTH		2

/* Commands are 64 bytes, aligned on a cache line boundary */
struct command {
    uint8_t  op;         // 0
    uint8_t  pad[63];    // 1-63
} PACKED;

sizecheck_struct( command, 64 );

#define COMMAND_INIT_PROCESS 0
struct command_init_process {
    uint8_t  op;                         // 0
    uint8_t  process_index;              // 1
    uint16_t pad;                        // 2
    uint16_t pid;                        // 4
    uint16_t jid;                        // 6
    uint16_t num_pendings;               // 8
    uint16_t num_memds;                  // 10
    uint16_t num_eqcbs;                  // 12
    uint16_t pending_tx_limit;           // 14
    uint32_t pending_table_addr;         // 16
    uint32_t up_pending_table_addr;      // 20
    uint32_t up_pending_table_ht_addr;   // 24
    uint32_t memd_table_addr;            // 28
    uint32_t eqcb_table_addr;            // 32
    uint32_t shdr_table_ht_addr;         // 36
    uint32_t result_block_addr;          // 40
    uint32_t eqheap_addr;                // 44
    uint32_t eqheap_length;              // 48
    uint32_t smb_table_addr;             // 52
    uint32_t uid;                        // 56
} PACKED;


#define COMMAND_MARK_ALIVE 1
struct command_mark_alive {
    uint8_t  op;             // 0
    uint8_t  process_index;  // 1
} PACKED;


#define COMMAND_INIT_EQCB 2
struct command_init_eqcb {
    uint8_t  op;            // 0
    uint8_t  pad;           // 1
    uint16_t eqcb_index;    // 2
    uint32_t base;          // 4
    uint32_t count;         // 8
} PACKED;

#define COMMAND_IP_TX 13
struct command_ip_tx {
    uint8_t  op;            // 0
    uint8_t  pad;           // 1
    uint16_t nid;           // 2
    uint16_t length;        // 4 (qb-1)
    uint16_t pad2;          // 6
    uint64_t address;       // 8
    uint16_t pending_index; // 16
} PACKED;


typedef uint32_t result_t;


/*
 * The mailbox structure is shared between the PPC and the Opteron.
 * It is uncached as a result, which causes significant slow downs
 * for any reads of the members.  Writes are fairly fast.
 */
struct mailbox
{
    volatile struct command     commandq[COMMAND_Q_LENGTH]; //    0
    volatile result_t           resultq[RESULT_Q_LENGTH];   // 4032

    volatile uint32_t           resultq_read;               // 4040
    volatile uint32_t           resultq_write;              // 4044
    volatile uint32_t           commandq_write;             // 4048
    volatile uint32_t           commandq_read;              // 4052

} PACKED PAGE_ALIGNED;  /* align to a page boundary so mailboxes can be
                         * securely mapped into user-level address spaces. */

sizecheck_struct( mailbox, 4096 );

struct mailbox * const seastar_mailbox = (void*)( seastar_virt_base + seastar_mailbox_base );


/** Map a physical address into the seastar memory.
 * 
 * The PPC can only directly address 256 MB of host memory per slot in
 * its Hypertransport map.  This writes the physical address into the
 * given slot.
 *
 * With our limited address space we start the mapping at
 * seastar_host_region_phys.
 */
static void
seastar_map_host_region(
	void *			addr
)
{
	// Round addr to the nearest 128 MB
	paddr_t			raw_paddr = __pa( addr );
	paddr_t			paddr = raw_paddr & ~( (1<<28) - 1 );

	htb_map[ 8 ] = HTB_VALID_FLAG | ((paddr >> 28) + 0);
	htb_map[ 9 ] = HTB_VALID_FLAG | ((paddr >> 28) + 1);

	seastar_host_region_phys	= paddr;

	printk( "%s: virt %p phys %p -> %p\n",
		__func__,
		addr,
		(void*) raw_paddr,
		(void*) seastar_host_region_phys
	);
}


/** Translate a host physical address into a firmware physical address.
 *
 * Since the PPC can only address a limited region of memory, it is necessary
 * to transform it from a host physical address to something that is mapped
 * via the htb_map on the PPC.
 */
static inline uint32_t
phys_to_fw(
	paddr_t			addr
)
{
	addr -= seastar_host_region_phys;
	addr &= (2<<28) - 1;
	return addr + (8<<28);
}


/** Convert a kernel virtual address to a Seastar accessible address */
static inline uint32_t
virt_to_fw(
	void *			addr
)
{
	return phys_to_fw( __pa( addr ) );
}


/** Convert a kernel virtual address to a hypertransport address. */
static inline uintptr_t
htaddr(
	void *			addr
)
{
	return __pa( addr ) >> 2;
}


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
 */
struct pending
{
	uint32_t		message;        // 0
	volatile uint32_t	status;         // 4
	struct pbuf *		pbuf;            // 8
	uint8_t			pad[ 128 - 16 ]; // 16
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
	pending->pbuf = (void*) pending_free_list;
	pending_free_list = pending;
}


/** Allocate a pending from the free list */
static inline struct pending *
seastar_pending_alloc( void )
{
	struct pending * next = pending_free_list;
	if( !next )
		panic( "%s: No free pendings?\n", __func__ );

	pending_free_list = (void*) next->pbuf;
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
		pending->status = 0;
		pending->pbuf = 0;
		seastar_pending_free( pending );
	}
}

#define NUM_EQ		1024
uint32_t eq[ NUM_EQ ];
unsigned eq_read;


result_t
seastar_cmd(
	const struct command *	cmd,
	int			wait
)
{
	struct mailbox * mb = &seastar_mailbox[ GENERIC_PROCESS_INDEX - 1 ];
	uint32_t head = mb->commandq_write;
	mb->commandq[ head ] = *cmd;
	mb->commandq_write = ( head >= COMMAND_Q_LENGTH - 1 ) ? 0 : head+1;

	if(0)
	printk( "%s: sent command %d in index %d (%p)\n",
		__func__,
		cmd->op,
		head,
		&mb->commandq[head]
	);
	if( !wait )
		return 0;

	uint32_t tail = mb->resultq_read;
	if(0)
	printk( "%s: waiting for results in index %d (head %d)\n",
		__func__,
		tail,
		mb->resultq_write
	);
	while( tail == mb->resultq_write )
		;

	result_t result = mb->resultq[ tail ];
	mb->resultq_read = ( tail >= RESULT_Q_LENGTH - 1 ) ? 0 : tail+1;

	return result;
}


/** Fetch the next event from the eq.
 *
 * Updates the read pointer to indicate that we have processed
 * the event.
 *
 * \return 0 if there is no event pending.
 */
static inline uint32_t
seastar_next_event( void )
{
	uint32_t e = eq[ eq_read ];
	if( !e )
		return 0;

	eq[ eq_read ] = 0;
	eq_read = (eq_read + 1) % NUM_EQ;
	return e;
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
static void
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


/** \group Convert an IP address to and from a Seastar NID.
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



static int
seastar_hw_init( void )
{
	uint32_t lower_memory = seastar_host_base;

	printk( "%s: Looking for niccb %p\n",
		__func__,
		niccb
	);

	// Read the NID from the localbus and write it into the niccbc
	uint16_t * const lb_tx_source
		= (void*)( seastar_virt_base + seastar_tx_source );
	niccb->local_nid = *lb_tx_source;

	printk( "%s: nid %d (0x%x) version %x built %x\n",
		__func__,
		niccb->local_nid,
		niccb->local_nid,
		niccb->version,
		niccb->build_time
	);

	// Allocate the PPC memory
	uint32_t lower_pending = lower_memory;
	lower_memory += NUM_PENDINGS * 32; // sizeof(struct pending)

	const int num_eq = 1;
	uint32_t lower_eqcb = lower_memory;
	lower_memory = num_eq * 32; // sizeof(struct eqcb)
	

	result_t result;

	// Initialize the HTB map so that the Seastar can see our memory
	// Since we are only doing upper pendings, we just use the
	// upper_pending_phys instead of the host_phys area
	seastar_map_host_region( upper_pending );

	// Attempt to send a setup command to the NIC
	struct command_init_process init_cmd = {
		.op			= COMMAND_INIT_PROCESS,
		.process_index		= GENERIC_PROCESS_INDEX,
		.uid			= 0,
		.jid			= 0,

		.num_pendings		= NUM_PENDINGS,
		.pending_tx_limit	= 0, // no tx pendings for ip
		.pending_table_addr	= lower_pending,
		.up_pending_table_addr	= virt_to_fw( upper_pending ),
		.up_pending_table_ht_addr = 0, // not needed for ip
		
		.num_memds		= 0,
		.memd_table_addr	= 0,

		.num_eqcbs		= num_eq,
		.eqcb_table_addr	= lower_eqcb,
		.eqheap_addr		= virt_to_fw( eq ),
		.eqheap_length		= NUM_EQ * sizeof(eq[0]),

		.shdr_table_ht_addr	= 0,
		.result_block_addr	= 0,
		.smb_table_addr		= 0,
	};

	result = seastar_cmd( (struct command *) &init_cmd, 1 );
	if( result != 0 )
		panic( "%s: init_process returned %d\n", __func__, result );

	struct command_init_eqcb eqcb_cmd = {
		.op			= COMMAND_INIT_EQCB,
		.eqcb_index		= 0,
		.base			= virt_to_fw( eq ),
		.count			= NUM_EQ,
	};

	result = seastar_cmd( (struct command *) &eqcb_cmd, 1 );
	if( result != 1 )
		panic( "%s: init_eqcb returned %d\n", __func__, result );

	struct command_mark_alive alive_cmd = {
		.op			= COMMAND_MARK_ALIVE,
		.process_index		= GENERIC_PROCESS_INDEX,
	};

	result = seastar_cmd( (struct command *) &alive_cmd, 1 );
	if( result != 0 )
		panic( "%s: mark_alive returned %d\n", __func__, result );

	set_idtvec_handler( 0xEE, &seastar_interrupt );

	return 0;
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
	netif->hwaddr[0]	= (niccb->local_nid >> 0) & 0xFF;
	netif->hwaddr[1]	= (niccb->local_nid >> 8) & 0xFF;

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
	if( seastar_hw_init() < 0 )
		return;

	struct ip_addr ipaddr = nid2ip( niccb->local_nid );
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
