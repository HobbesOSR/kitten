/** \file
 * Layout of Seastar memory and conversion routines.
 */
#ifndef _seastar_memory_h_
#define _seastar_memory_h_

#define SEASTAR_MTU		8192

#define HTB_MAP			0x000020000
#define HTB_VALID_FLAG		0x8000


/** \name Addresses of important data structures in Seastar memory.
 *
 * These are the offets in Seastar memory of the important datastructures
 * that we need to initialize the hardware and send commands to it.
 * @{
 */
#define seastar_tx_source	0xFFE00108
#define seastar_phys_base	0xFFFA0000
#define seastar_mailbox_base	0xFFFA0000
#define seastar_skb_base	0xFFFA4000
#define seastar_host_base	0xFFFA5000
#define seastar_htb_base	0xFFE20000
#define seastar_niccb_base	0xFFFFE000

/** @} */

/** The Seastar is mapped at L4 entry 511, L3 entry 511, L2 entry 511,
 * giving us a 2 MB window at the very highest end of memory into the
 * Seastar and its memory mapped devices.
 */
#define seastar_virt_base ( 0xFFFFFFFFull << 32 )
//((uint8_t*) ~( (1<<20)-1 ))
//(511ull << 36) | (511ull << 29) | (511ull <<20) )

/** Location in host memory of NIC control block structure */
static struct niccb * const niccb
	= (void*)( seastar_virt_base + seastar_niccb_base );

/** 64-bit HT quad-word address of incoming datagram buffers */
static uint64_t * const seastar_skb
	= (void*)( seastar_virt_base + seastar_skb_base );

/** Location in host memory of PPC's hypertransport map */
static volatile uint32_t * const htb_map
	= (void*)( seastar_virt_base + seastar_htb_base );

/** Location in host memory of PPC's per-process mailbox structures */
static struct mailbox * const seastar_mailbox
	= (void*)( seastar_virt_base + seastar_mailbox_base );

/** Local NID */
extern unsigned local_nid;

/** Start of host memory buffer mapped into Seastar memory through HTB */
extern paddr_t seastar_host_region_phys;


/** Translate a host physical address into a firmware physical address.
 *
 * Since the PPC can only address a limited region of memory, it is necessary
 * to transform it from a host physical address to something that is mapped
 * via the htb_map on the PPC.
 *
 * \todo Check for invalid host memory addresses?
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


/* Return the next event */
extern uint32_t
seastar_next_event( void );


/* Bring up the low-level hardware */
extern int
seastar_hw_init(
	void *			upper_pending,
	unsigned		num_pendings
);


/* Hardware interrupt handler */
extern int
seastar_interrupt(
	unsigned		vector,
	void *			priv
);


#endif
