/*
 * Copyright (c) 2004, 2005 Cray Inc. and Sandia Corporation
 */
#ifndef _NICCB_H_
#define _NICCB_H_

#include <lwk/types.h>

typedef uint32_t pending_p;
#define PAD(k,n) uint8_t _unused_##k[n]



/*
 * This structure includes the information captured as the result of
 * Machine Check recovery in the ASM code. Do NOT re-order (or resize) these
 * fields without adjusting the ASM source.
 */
typedef struct {
    uint32_t		mcr_count;		// Recovery count
    uint32_t		last_mcsr;		// Last MCSR value
    uint32_t		timestamp[2];		// 64-bit Timestamp of last one
} mcr_data_t ;

/*
 * NOTE: Many of the fields in the niccb are "known" by entities outside
 * the firmware (e.g. RSMS/L0). Hence the size and location of many of these
 * fields should be considered "fixed" and generally should NOT be moved
 * without serious consideration.
 */

struct niccb
{
    uint32_t	       version;                 //   0
    uint32_t  	       food;			//   4
    uint32_t	       host_heartbeat;          //   8 This appears unused!!!
    volatile uint32_t  log_write;               //  12 (fwlog_entry_t *)
    volatile uint32_t  firmware_heartbeat;      //  16 (0x10)
    uint32_t	       host_base;               //  20 This too appears unused!
    uint32_t           stateless_drop_counter;  //  24
    uint32_t           build_time;              //  28
    volatile uint32_t  hostbase;                //  32 (0x20)

    /* Portals Counters */
    uint32_t           tx_complete;             //  36 
    uint32_t           rx_message;              //  40
    uint32_t           rx_complete;             //  44
    uint32_t           interrupt_count;         //  48 (0x30)
    uint32_t           sequence_errors;         //  52
    uint32_t           cam_in_use;              //  56
    uint32_t           cam_high_water;          //  60
    uint32_t           last_nid_drop;		//  64 (0x40)
    uint32_t           crash_cntl_word;		//  68
    uint32_t           crash_heartbeat;		//  72

    uint32_t           source_freelist;         //  76 (0x4C)
    uint32_t           packets_drained;         //  80
    uint32_t           tx_pending_head;         //  84 (0x54)
    pending_p          tx_pending;              //  88 (0x58)
    pending_p          tx_completion;           //  92 (0x5C)
    uint32_t           cam_ovf;                 //  96 (0x60)

    /* IP Counters */
    uint32_t           ip_tx;                   // 100 (0x64)
    uint32_t           ip_tx_drop;              // 104 (0x68)
    uint32_t           ip_rx;                   // 108 (0x6C) 
    uint32_t           ip_rx_drop;              // 112 (0x70)
    
    /*
     * These do not seem to be used by anything other than
     * the ssnal_nic_init routines, but we have them in here
     * for completness.
     */
    volatile uint32_t  rx_credit_write;         // 116 (0x74)
    volatile uint32_t  rx_credit_read;          // 120 (0x78)
    uint32_t           cam_assignment_head;     // 124  used only by cabload
    uint32_t           cam_assignment_map;      // 128  used only by cabload
    uint32_t           cam_freelist;            // 132 (0x84)
    uint32_t           smb_in_use;              // 136 (0x88)
    uint32_t           smb_high_water;          // 140 (0x8C)

    mcr_data_t         mcr_stats;		// 144 Machine Check Stats

    /* Mailbox words for L0 <--> Firmware interaction. See L0_mbox.h for
       details on contents and usage.
    */
    volatile uint32_t  L0_cmd;			// 160 Command word FROM L0
    volatile uint32_t  L0_resp;			// 164 Response word TO L0

    /* Very convenient to know who I am */
    uint16_t           local_nid;               // 168
    uint16_t           tx_vc_cntl;              // 170 Control flags for VC2
    PAD(4, 12);

    uint32_t           geq_nearly_full;		// 184 Times when Generic EQ was full
    /*
     * The firmware will pass the identity of the boot
     * node and the last message sequence number received
     * in this structure to allow synchronization after
     * high-speed boot.
     */
    uint16_t           beer_last_release;       // 188 (0xBC) boot toleration
    uint16_t           beer_boot_nid;           // 190 (0xBE) boot toleration

    /*
     * The Host OS writes information about a trace buffer in Opteron
     * memory into tracebuf_info.  The firmware does not need to understand
     * this information.  A program run on the SMW called ptltrace accesses
     * tracebuf_info to retrieve and decode a node's trace buffer.
     */
    uint8_t            tracebuf_info[32];       // 192 (0xC0)

/*
    volatile struct    pending boot_pending;    // 224 (0xE0)

    uint16_t	       test_nid_map[256];	// 256 (0x100)
*/
} __attribute__((packed,aligned));

#endif
