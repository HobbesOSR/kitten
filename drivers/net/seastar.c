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
#include <arch/page.h>
#include <arch/proto.h>
#include <net/seastar.h>

#define HTB_MAP			0x20000
#define HTB_VALID_FLAG		0x8000

#define seastar_phys_base	0xFFFA0000
#define seastar_mailbox_base	0xFFFA0000
#define seastar_skb_base	0xFFFA4000
#define seastar_host_base	0xFFFA5000
#define seastar_htb_base	0xFFE20000
#define seastar_niccb_base	0xFFFFE000

#define seastar_virt_base ( 0xFFFFFFFFull << 32 )
//((uint8_t*) ~( (1<<20)-1 ))
//(511ull << 36) | (511ull << 29) | (511ull <<20) )

static struct niccb * const niccb
	= (void*)( seastar_virt_base + seastar_niccb_base );

static uint64_t * const seastar_skb
	= (void*)( seastar_virt_base + seastar_skb_base );

static volatile uint32_t * const htb_map
	= (void*)( seastar_virt_base + seastar_htb_base );



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
} PACKED ALIGNED;

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
} PACKED ALIGNED;


#define COMMAND_MARK_ALIVE 1
struct command_mark_alive {
    uint8_t  op;             // 0
    uint8_t  process_index;  // 1
} PACKED ALIGNED;


#define COMMAND_INIT_EQCB 2
struct command_init_eqcb {
    uint8_t  op;            // 0
    uint8_t  pad;           // 1
    uint16_t eqcb_index;    // 2
    uint32_t base;          // 4
    uint32_t count;         // 8
} PACKED ALIGNED;


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


/*
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
 * \note To the best of my knowledge these are unused for IP.
 * Only the lower pendings need to be allocated, but the firmware
 * will complain if there are no upper pendings.
 */
struct pending
{
	uint32_t		message;        // 0
	volatile uint32_t	status;         // 4
	uint32_t		skb;            // 8
	uint8_t			pad[ 128 - 12 ]; // 12
} PACKED ALIGNED;

sizecheck_struct( pending, 128 );


#define NUM_SKB		64
#define NUM_PENDINGS	64
struct pending upper_pending[ NUM_PENDINGS ];
uint8_t skb[ NUM_SKB ][ 65536 ];

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

	printk( "%s: sent command %d in index %d (%p)\n",
		__func__,
		cmd->op,
		head,
		&mb->commandq[head]
	);
	if( !wait )
		return 0;

	uint32_t tail = mb->resultq_read;
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


/** Start of host memory buffer mapped into Seastar memory through HTB */
paddr_t seastar_host_region_phys;


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


static inline uint32_t
virt_to_fw(
	void *			addr
)
{
	return phys_to_fw( __pa( addr ) );
}


void
seastar_refill_skb( void )
{
	int i;
	for( i=0 ; i < NUM_SKB ; i++ )
		seastar_skb[i] = __pa( &skb[i] ) >> 2;

	for( i=0 ; i < NUM_PENDINGS ; i++ )
		upper_pending[i].status = 0;
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


static void
seastar_ip_rx(
	const unsigned		index
)
{
#if 0
	// Extract the IP datagram from pending
	struct pending * const p = &upper_pending[ index ];
	printk( "message %d status %d skb %d\n",
		p->message,
		p->status,
		p->skb
	);
#endif

	uint8_t * const data = &skb[ index ][0];
	if( index> NUM_SKB )
	{
		printk( KERN_ERR
			"%s: Invalid skb %d from Seastar!\n",
			__func__,
			index
		);
		return;
	}

	int i;
	char buf[ 128 ];
	size_t offset = 0;
	for( i=0 ; i<32 ; i++ )
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
	printk( "***** %s: We get signal!\n", __func__ );
	int ev_count = 0;

	while( 1 )
	{
		uint32_t e = seastar_next_event();
		if( !e )
			break;

		const unsigned type	= (e >> 16) & 0xFFFF;
		const unsigned index	= (e >>  0) & 0xFFFF;

		printk( "%d: event %d pending %d\n", ev_count, type, index );
		ev_count++;

		if( type != 126 )
		{
			printk( KERN_NOTICE
				"***** %s: unhandled event type %d\n",
				__func__,
				type
			);
			continue;
		}

		seastar_ip_rx( index );
	}

	printk( "%s: processed %d events\n", __func__, ev_count );
}


void
seastar_init( void )
{
	uint32_t lower_memory = seastar_host_base;

	printk( "%s: Looking for niccb %p\n",
		__func__,
		niccb
	);

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

	seastar_refill_skb();

	set_idtvec_handler( 0xEE, &seastar_interrupt );
}


driver_init( "net", seastar_init );
