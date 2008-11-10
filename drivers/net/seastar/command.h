/** \file
 * Commands and structures to interface with the Seastar.
 */
#ifndef _seastar_cmd_h_
#define _seastar_cmd_h_

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


/** Shared mailbox structure.
 *
 * The mailbox structure is shared between the PPC and the Opteron.
 * It is uncached as a result, which causes significant slow downs
 * for any reads of the members.  Writes are fairly fast.
 *
 * align to a page boundary so mailboxes can be
 * securely mapped into user-level address spaces.
 */
struct mailbox
{
    volatile struct command     commandq[COMMAND_Q_LENGTH]; //    0
    volatile result_t           resultq[RESULT_Q_LENGTH];   // 4032

    volatile uint32_t           resultq_read;               // 4040
    volatile uint32_t           resultq_write;              // 4044
    volatile uint32_t           commandq_write;             // 4048
    volatile uint32_t           commandq_read;              // 4052

} PACKED PAGE_ALIGNED;

sizecheck_struct( mailbox, 4096 );


/* Send a command to the Seastar. */
extern result_t
seastar_cmd(
	const struct command *	cmd,
	int			wait_for_reply
);



#endif
