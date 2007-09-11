/*
 * Copyright (c) 2006 Cray, Inc.
 *
 * The contents of this file is proprietary information of Cray Inc. 
 * and may not be disclosed without prior written consent.
 */
/*
 *
 * This code is licensed under the GNU General Public License,
 * Version 2.  See the file COPYING for more details.
 */


#ifndef __L0RCA_CONFIG_H__
#define __L0RCA_CONFIG_H__


#include <rca/cray_event_def.h>		/* rca_types.h includes rs_event.h */

/*
 * PKT_MODE* Registers:
 *
 * Writing a "0" to a bit in pkt_mode0 will atomically clear that bit
 * in pkt_mode.  Writing a "1" to a bit in pkt_mode1 will atomically
 * set that bit in pkt_mode.  Reading any of these will return the
 * instantanious contents of pkt_mode.  Writing pkt_mode replaces the
 * contents non-atomically.
 */
typedef struct {
    uint32_t    pkt_mode;
    uint32_t    _pad1;
    uint32_t    pkt_mode0;  /* atomically clear bits */
    uint32_t    _pad2;
    uint32_t    pkt_mode1;  /* atomically set bits */
} l0ssi_pkt_mode_t;

#define phy_pkt_mode    (*(l0ssi_pkt_mode_t *)PKT_MODE)

#define PKT_MODE0   (uint32_t)&phy_pkt_mode.pkt_mode0
#define PKT_MODE1   (uint32_t)&phy_pkt_mode.pkt_mode1

/*
 * How the RCA interrupts the L0.
 *
 * Writing a "0" to a bit in pkt_mode0 will atomically CLEAR that bit
 * in pkt_mode.  Writing a "1" to a bit in pkt_mode1 will atomically
 * SET that bit in pkt_mode.  Normally, pkt_mode should never be
 * written directly.  It can be read to get the current state of bits.
 * In general, any set bits in pkt_mode will cause an interrupt to
 * be raised to the L0, via the SSI and L0_FPGA.
 *
 * CAUTION: pkt_mode0 must only be written with "0"(s) in the
 * position(s) to be cleared and "1"s everywhere else.  I.e. it
 * must be written with the value one would use to AND-out the
 * bits.  This is contrary to most SET/CLEAR register implementations.
 *
 * Bits in pkt_mode are assigned in l0ssi_intr.h
 */
typedef l0ssi_pkt_mode_t l0rca_intr_t;
#define l0r_intr_get	pkt_mode
#define l0r_intr_clr	pkt_mode0
#define l0r_intr_set	pkt_mode1

/* Should be removed... */
#define L0RCA_CONFIG L0RCA_CFG

/* defined channel id */
#define L0RCA_CH_EV_UP        0
#define L0RCA_CH_EV_DOWN      1
#define L0RCA_CH_CON_UP       2
#define L0RCA_CH_CON_DOWN     3
#define L0RCA_CH_KGDB_UP      4
#define L0RCA_CH_KGDB_DOWN    5
#define NUM_L0RCA_CHANNELS    6

/* NOTE for the following L0 Opteron communication related structures:
 * ###################################################################
 * There are following restrictions for the L0 Opteron communication
 * related structures.
 * The elements must be aligned on 4-byte boundaries.  The structure
 * size must be a multiple of 4 bytes. Structures should be packed so
 * that the compiler will not insert padding.  
 * ###################################################################
 */

/* 
 * l0rca_ch_data_t: channel buffer data structure
 * NOTE: This l0rca_ch_data_t is packed.  If we update this structure,
 * 	we need to make sure that each element is 4-byte aligned,
 * 	otherwise it might break the L0 Opteron communication (size of
 * 	l0rca_ch_data_t must be a multiple of 4bytes).
 *	All communication channel uses rs_event_t so, the size of object
 *	in buffer is sizeof(rs_event_t).  RCA events has fixed ev_data
 *	length (256) and num_obj is the number of events can be stored
 *	in the buffer.
 *
 * The *_intr_bit fields declare which bit in the PKT_MODE register
 * is used for the channel interrupt.  l0_intr_bit is for interrupts
 * sent *to* the L0, while proc_intr_bit is for interrupts sent *to*
 * the Opteron processor.
 */
typedef struct l0rca_ch_data_s {
	uint32_t num_obj;        /* number of objects    */
	uint32_t ridx;           /* read index           */
	uint32_t widx;           /* write index          */
	uint32_t l0_intr_bit;	 /* Opteron -> L0 intr assignment */
	uint32_t proc_intr_bit;	 /* L0 -> Opteron intr assignment */
} __attribute__((packed)) l0rca_ch_data_t;

#define L0RCA_CONF_VERSION 	2

/*
 * Circular Buffer Usage:
 *
 * When (widx == ridx), buffer is empty;
 * ELSE When (widx - ridx < num_obj)), there are one or more 
 * available buffers.
 *
 * ridx and widx reflect the object index, not byte index.
 *
 * Restrictions:
 *
 * num_obj must be a power of 2 (i.e. (num_obj & (num_obj - 1)) == 0).
 * Therefore indices are normalized with AND: idx = ridx & (num_obj - 1).
 *
 */

/*
 * NOTE: This l0rca_config_t is packed.  If we update this sturcture,
 * 	we need to make sure that each element is 4-byte aligned,
 * 	otherwise it might break the L0 Opteron communication (size
 * 	of l0rca_config_t must be a multiple of 4bytes).
 *
 * configuration data structure */
typedef struct l0rca_config_s {
	uint64_t        l0rca_buf_addr;         /* ch buffer addr */
	uint64_t        l0rca_l0_intr_addr;     /* interrupt to L0 */
	uint32_t		version;				/* config version */
	rs_node_t		proc_id;       			 /* node id */
	int32_t         proc_num;     			  /* proc number (0-3) */
	int32_t         reserved_1;   			  /* reserved for future use */
	int32_t         reserved_2;    			 /* reserved for future use */
	/* channel data */
	l0rca_ch_data_t chnl_data[NUM_L0RCA_CHANNELS];
} __attribute__((packed)) l0rca_config_t;


/*
 * Definitions in the L0-reserved area of SIC RAM
 */
#define L0_SIC_RAM  0xfffff000
#define L0_SIC_RAM_LEN  4096

#define COLDSPIN    0xfffffe80
#define L0RCA_CFG   L0_SIC_RAM
#define L0RCA_CFG_LEN   (COLDSPIN - L0RCA_CFG)

/*
 * The following provides an abstraction for accessing the RAM
 * location of the config structure.  It set phy_l0r_cfg to the
 * l0rca_config_t physical address defined as L0RCA_CONFIG.
 * To use it, include this header file and then access the
 * config area as &phy_l0r_cfg.<element>.
 */
#define phy_l0r_cfg	(*(l0rca_config_t *)L0RCA_CFG)


#endif /* !__L0RCA_CONFIG_H__ */
