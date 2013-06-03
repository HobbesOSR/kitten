/* Copyright (c) 2009, Sandia National Laboratories */

/*-
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/pci/pcivar.h,v 1.80.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $
 *
 */

#ifndef _LWK_PCICFG_H
#define _LWK_PCICFG_H

#include <lwk/pci/pcicfg_regs.h>


/* Some PCI bus constants */
#define	PCI_DOMAINMAX	65535	// highest supported domain number
#define	PCI_BUSMAX	255	// highest supported bus number
#define	PCI_SLOTMAX	31	// highest supported slot number
#define	PCI_FUNCMAX	7	// highest supported function number
#define	PCI_REGMAX	255	// highest supported config register addr
#define PCI_MAXHDRTYPE  2	// max. valid header type

#define	PCI_MAXMAPS_0	6	// max. no. of memory/port maps
#define	PCI_MAXMAPS_1	2	// max. no. of maps for PCI to PCI bridge
#define	PCI_MAXMAPS_2	1	// max. no. of maps for CardBus bridge


typedef uint64_t	pci_mem_addr_t;
typedef uint32_t	pci_io_addr_t;


/** PCI power management info.
 *
 * This structure stores a device's PCI power management information.
 */
typedef struct pcicfg_pp {
	uint16_t	pp_cap;			//!< power management capabilities
	uint8_t		pp_status;		//!< config space address of PCI power status reg
	uint8_t		pp_pmcsr;		//!< config space address of PMCSR reg
	uint8_t		pp_data;		//!< config space address of PCI power data reg
} pcicfg_pp_t;


/** PCI Vital Product Data (VPD) read-only entry. */
typedef struct vpd_ro {
	char		keyword[2];		//!< the 2 byte "keyword"
	char *		value;			//!< the vpd value
} vpd_ro_t;


/** PCI Vital Product Data (VPD) read/write entry.
 *
 * Read/write VPD entries can be used to stash away a bit of
 * device instance specific info, e.g., an asset tag identifier.
 */
typedef struct vpd_rw {
	char		keyword[2];		//!< the 2 byte "keyword"
	char *		value;			//!< the vpd value
	int		start;			//!< vpd address of this entry
	int		len;			//!< length of this entry in bytes
} vpd_rw_t;


/** PCI Vital Product Data (VPD).
 *
 * Vital product data is used to store things like serial numbers,
 * service tag numbers, etc.  It appears to be generally ancillary info
 * not necessarily needed to configure the device.
 */
typedef struct pcicfg_vpd {
	uint8_t		vpd_reg;		//!< base register, + 2 for addr, + 4 data
	char		vpd_cached;		//!< cached value
	char *		vpd_ident;		//!< string identifier
	int		vpd_ro_count;		//!< count of VPD read-only fields 
	vpd_ro_t *	vpd_ro;			//!< array of VPD read-only fields
	int		vpd_rw_count;		//!< count of VPD read/write fields 
	vpd_rw_t *	vpd_rw;			//!< array of VPD read/write fields 
} pcicfg_vpd_t;


/** PCI Message Signaled Interrupt (MSI) info.
 *
 * MSI allows a device to request service by writing a "message" to
 * an address being monitored by the host's interrupt logic.  The message
 * payload contains the source of the interrupt.
 */
typedef struct pcicfg_msi {
	uint16_t	msi_ctrl;		//!< message control
	uint8_t		msi_location;		//!< offset of MSI capability registers
	uint8_t		msi_msgnum;		//!< number of messages
	int		msi_alloc;		//!< number of allocated messages
	uint64_t	msi_addr;		//!< contents of address register
	uint16_t	msi_data;		//!< contents of data register
} pcicfg_msi_t;


typedef struct msix_vector {
	uint64_t	mv_address;		//!< contents of address register
	uint32_t	mv_data;		//!< contents of data register
	int		mv_irq;
} msix_vector_t;


typedef struct msix_table_entry {
	u_int		mte_vector;		//!< 1-based index into msix_vectors array
	u_int		mte_handlers;
} msix_table_entry_t;


/** PCI Message Signaled Interrupt Extended (MSI-X) info.
 *
 * MSI-X extends MSI by allowing a larger number of interrupt vectors.
 * A device can support both MSI and MSI-X but only one can be enabled
 * at a time.
 */
typedef struct pcicfg_msix {
	uint16_t	msix_ctrl;		//!< message control
	uint16_t	msix_msgnum;		//!< number of messages
	uint8_t		msix_location;		//!< offset of MSI-X capability registers
	uint8_t		msix_table_bar;		//!< BAR containing vector table
	uint8_t		msix_pba_bar;		//!< BAR containing PBA
	uint32_t	msix_table_offset;	//!< offset of msix table
	uint32_t	msix_pba_offset;	//!< offset of pba
	int		msix_alloc;		//!< number of allocated vectors
	int		msix_table_len;		//!< length of virtual table
	msix_table_entry_t *	msix_table;	//!< virtual table
	msix_vector_t *	msix_vectors;		//!< array of allocated vectors
} pcicfg_msix_t;


/** PCI HyperTransport info. */
typedef struct pcicfg_ht {
	uint8_t		ht_msimap;		//!< Offset of MSI mapping cap registers
	uint16_t	ht_msictrl;		//!< MSI mapping control
	uint64_t	ht_msiaddr;		//!< MSI mapping base address
} pcicfg_ht_t;


/** PCI configuration header information.
 *
 * Every device on the PCI bus, including bridges, has an associated
 * configuration header in the PCI configuration address space.  Since
 * this space is not easily accessible and it is mostly read only, this
 * structure is used to mirror a device's config hdr info in normal
 * memory.
 */
typedef struct pcicfg_hdr {
	uint32_t	bar[PCI_MAXMAPS_0];	//!< base Address Registers (BARs)
	uint32_t	bios;			//!< the BIOS mapping (??)

	uint16_t	sub_vendor;		//!< card vendor ID
	uint16_t	sub_device;		//!< card device ID, assigned by card vendor
	uint16_t	vendor_id;		//!< chip vendor ID
	uint16_t	device_id;		//!< chip device ID, assigned by chip vendor

	uint16_t	command_reg;		//!< disable/enable chip and PCI options
	uint16_t	status_reg;		//!< supported PCI features and error state

	uint8_t		base_class;		//!< chip PCI class
	uint8_t		sub_class;		//!< chip PCI sub-class
	uint8_t		prog_iface;		//!< chip PCI programming interface
	uint8_t		rev_id;			//!< chip revision ID

	uint8_t		hdr_type;		//!< chip config header type
	uint8_t		cache_line_sz;		//!< cache line size in 4byte units
	uint8_t		interrupt_pin;		//!< PCI interrupt pin
	uint8_t		interrupt_line;		//!< interrupt line (IRQ for PC arch)

	uint8_t		min_grant;		//!< min. useful bus grant time in 250ns units
	uint8_t		max_latency;		//!< max. tolerated bus grant latency in 250ns
	uint8_t		latency_timer;		//!< latency timer in units of 30ns bus cycles

	uint8_t		is_multi_func;		//!< multi-function device (from hdr_type reg)
	uint8_t		num_maps;		//!< actual number of PCI BAR maps used

	uint32_t	domain;			//!< PCI domain
	uint8_t		bus;			//!< config space bus address
	uint8_t		slot;			//!< config space slot address
	uint8_t		func;			//!< config space function number

	pcicfg_pp_t	pp;			//!< power management info
	pcicfg_vpd_t	vpd;			//!< vital product Data
	pcicfg_msi_t	msi;			//!< message signaled interrupt info
	pcicfg_msix_t	msix;			//!< message signaled intterupt extended info
	pcicfg_ht_t	ht;			//!< HyperTransport
} pcicfg_hdr_t;


/** Additional config header info for type 1 devices (PCI-to-PCI bridges).
 *
 * This structure stores the PCI configuration header information for a
 * PCI-to-PCI bridge that is not already covered by 'struct pcicfg_hdr'.
 */
typedef struct pcicfg_hdr1 {
	uint8_t		primary_bus_num;	//!< upstream bus number
	uint8_t		secondary_bus_num;	//!< downstream bus number
	uint8_t		subordinate_bus_num;	//!< highest downstream bus number

	pci_mem_addr_t	mem_base;		//!< base address of non-prefetchable memory window
	pci_mem_addr_t	mem_limit;		//!< topmost address of non-prefetchable memory window
	pci_mem_addr_t	mem_pfetch_base;	//!< base addr of prefetchable memory
	pci_mem_addr_t	mem_pfetch_limit;	//!< topmost address of prefetchable memory
	pci_io_addr_t	io_base;		//!< base address of port window
	pci_io_addr_t	io_limit;		//!< topmost address of port window

	uint8_t		sec_latency_timer;	//!< secondary latency timer
	uint16_t	sec_status_reg;		//!< secondary bus status register
	uint16_t	bridge_ctrl_reg;	//!< bridge control register
} pcicfg_hdr1_t;


uint32_t pcicfg_read(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	unsigned int	reg,
	unsigned int	width
);


void
pcicfg_write(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	unsigned int	reg,
	unsigned int	width,
	uint32_t	value
);


void
pcicfg_hdr_read(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	pcicfg_hdr_t *	hdr
);


void
pcicfg_hdr1_read(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	pcicfg_hdr1_t *	hdr1
);


#endif
