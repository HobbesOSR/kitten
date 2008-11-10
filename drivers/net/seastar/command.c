/** \file
 * Low-level interface to the Seastar hardware.
 */
#include <lwk/driver.h>
#include <lwk/signal.h>
#include <lwk/netdev.h>
#include <arch/page.h>
#include <arch/proto.h>
#include "niccb.h"
#include "memory.h"
#include "command.h"


/** NID of this node */
unsigned local_nid;

/** Start of host memory buffer mapped into Seastar memory through HTB */
paddr_t seastar_host_region_phys;

#define NUM_EQ		1024
uint32_t eq[ NUM_EQ ];
unsigned eq_read;


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
	const void *		addr
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



/** Send a command to the Seastar.
 *
 * The Seastar has a command queue per process that is used to
 * send rx, tx and setup commands to the device.  Some commands
 * return a value on the result queue.
 *
 * \todo Implement a caching structure for the read/write indices
 * to avoid the expensive round-trip reads from the Seastar.
 *
 * \todo Check for a full commandq before writing!
 */
result_t
seastar_cmd(
	const struct command *	cmd,
	int			wait_for_result
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
	if( !wait_for_result )
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
uint32_t
seastar_next_event( void )
{
	uint32_t e = eq[ eq_read ];
	if( !e )
		return 0;

	eq[ eq_read ] = 0;
	eq_read = (eq_read + 1) % NUM_EQ;
	return e;
}



/** Bring up the low-level Seastar hardware.
 *
 * This also sets the node's NID.
 */
int
seastar_hw_init(
	void * const		upper_pending,
	unsigned		num_pendings
)
{
	uint32_t lower_memory = seastar_host_base;

	printk( "%s: Looking for niccb %p\n",
		__func__,
		niccb
	);

	// Read the NID from the localbus and write it into the niccbc
	uint16_t * const lb_tx_source
		= (void*)( seastar_virt_base + seastar_tx_source );
	niccb->local_nid = local_nid = *lb_tx_source;

	printk( "%s: nid %d (0x%x) version %x built %x\n",
		__func__,
		niccb->local_nid,
		niccb->local_nid,
		niccb->version,
		niccb->build_time
	);

	// Allocate the PPC memory
	uint32_t lower_pending = lower_memory;
	lower_memory += num_pendings * 32; // sizeof(struct pending)

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

		.num_pendings		= num_pendings,
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
