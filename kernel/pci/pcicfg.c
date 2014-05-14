/* Copyright (c) 2009, Sandia National Laboratories */

/*
 * Copyright (c) 1997, Stefan Esser <se@freebsd.org>
 * Copyright (c) 2000, Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2000, BSDi
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
 */

#include <lwk/kernel.h>
#include <lwk/pci/pcicfg.h>
#include <arch/pci/pcicfg.h>


/* Convenience macros for reading/writing PCI configuration space */
#define RREG(r,w)	pcicfg_read(bus, slot, func, (r), (w))
#define WREG(r,v, w)	pcicfg_write(bus, slot, func, (r), (w), (v))


/** Verifies that the PCI address given is valid. */
static void
validate_addr(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	unsigned int	reg,
	unsigned int	width
)
{
	if (bus > PCI_BUSMAX)
		panic("PCI bus address out of range\n");

	if (slot > PCI_SLOTMAX)
		panic("PCI slot address out of range\n");

	if (func > PCI_FUNCMAX)
		panic("PCI function ID out of range\n");

	if (reg > PCI_REGMAX)
		panic("PCI register ID out of range\n");

	if ((width != 1) && (width != 2) & (width != 4))
		panic("PCI register width invalid\n");

	if ((reg & (width - 1)) != 0)
		panic("PCI unaligned register access\n");
}


uint32_t
pcicfg_read(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	unsigned int	reg,
	unsigned int	width
)
{
	validate_addr(bus, slot, func, reg, width);
	return arch_pcicfg_read(bus, slot, func, reg, width);
}


void
pcicfg_write(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	unsigned int	reg,
	unsigned int	width,
	uint32_t	value
)
{
	validate_addr(bus, slot, func, reg, width);
	arch_pcicfg_write(bus, slot, func, reg, width, value);
}


/** Adjusts PCI 1.0 devices to match PCI >= 2.0 standards. */
static void
pcicfg_fixancient(
	pcicfg_hdr_t *	hdr
)
{
	/* PCI-to-PCI bridges should use header type 1 */
	if ( (hdr->hdr_type == 0)
		&& (hdr->base_class == PCIC_BRIDGE)
		&& (hdr->sub_class  == PCIS_BRIDGE_PCI) )
	{
		printk(KERN_WARNING "Encountered ancient PCI 1.0 device.");
		hdr->hdr_type = 1;  /* fixup */
	}
}


/** Reads header type specific data. */
static void
pcicfg_hdrtypedata(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	pcicfg_hdr_t *	hdr
)
{
	switch (hdr->hdr_type) {
	case 0:  /* Standard Device Header */
		hdr->sub_vendor  =  RREG( PCIR_SUBVEND_0, 2 );
		hdr->sub_device  =  RREG( PCIR_SUBDEV_0,  2 );
		hdr->num_maps    =  PCI_MAXMAPS_0;
		break;
	case 1:  /* PCI-to-PCI Bridge Header */
		hdr->num_maps    =  PCI_MAXMAPS_1;
		break;
	case 2: /* Cardbus Header */
		hdr->sub_vendor  =  RREG( PCIR_SUBVEND_2, 2 );
		hdr->sub_device  =  RREG( PCIR_SUBDEV_2,  2 );
		hdr->num_maps    =  PCI_MAXMAPS_2;
		break;
	}
}


/** Reads PCI extended capabilities. */
static void
pcicfg_read_extcap(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	pcicfg_hdr_t *	hdr
)
{
	uint32_t	val;
	int		ptr, nextptr, ptrptr;

	/*
	 * Start out with all structures invalid.
	 * For each extended capability found, the corresponding structure's
	 * valid bit will be set to true.
	 */
	hdr->pp.valid   = false;
	hdr->msi.valid  = false;
	hdr->msix.valid = false;
	hdr->pcix.valid = false;
	hdr->msix.valid = false;

	/* Figure out where to start */
	switch (hdr->hdr_type) {
	case 0:
	case 1:  ptrptr = PCIR_CAP_PTR;   break;
	case 2:  ptrptr = PCIR_CAP_PTR_2; break;
	default: return;  /* no extended capabilities support */
	}

	/* Read all entries in the capability list */
	nextptr = RREG( ptrptr, 1 );
	while (nextptr != 0)
	{
		/* Sanity check */
		if (nextptr > 255) {
			printk(KERN_WARNING
				"Illegal PCI extended capability offset %d.",
				nextptr
			);
			return;
		}

		/* Find the next entry */
		ptr     = nextptr;
		nextptr = RREG( ptr + PCICAP_NEXTPTR, 1 );

		/* Process this entry */
		switch (RREG( ptr + PCICAP_ID, 1) ) {

		case PCIY_PMG:
			hdr->pp.valid  = true;
			hdr->pp.pmc   = RREG( ptr + PCIR_POWER_CAP,    2 );
			hdr->pp.pmcsr = RREG( ptr + PCIR_POWER_STATUS, 2 );
			break;

		case PCIY_PCIX:
			hdr->pcix.valid   = true;
			hdr->pcix.command = RREG( ptr + PCIXR_COMMAND, 2 );
			hdr->pcix.status  = RREG( ptr + PCIXR_STATUS,  4 );
			break;

                case PCIY_MSI:
                        hdr->msi.valid = true;
                        hdr->msi.msi_location = ptr;
                        hdr->msi.msi_ctrl = RREG(ptr + PCIR_MSI_CTRL, 2);
                        hdr->msi.msi_msgnum = 
                            1 << ((hdr->msi.msi_ctrl & PCIM_MSICTRL_MMC_MASK) >> 1);

                        /*
                        printk("MSI supports %d message%s%s%s \n", hdr->msi.msi_msgnum,
                                (hdr->msi.msi_msgnum == 1) ? "" : "s",
                                (hdr->msi.msi_ctrl & PCIM_MSICTRL_64BIT) ? ", 64 bit" : "",
                                (hdr->msi.msi_ctrl & PCIM_MSICTRL_VECTOR) ? ", vector masks" : "");
                                */

                        if (hdr->msi.msi_ctrl & PCIM_MSICTRL_MSI_ENABLE) {
                            /*
                            printk("Warning: MSI already enabled!!\n");
                            */
                        }

                        break;

		/* Extended Message Signaled Interrupt (MSI-X) */
		case PCIY_MSIX:	
			hdr->msix.valid             = true;
			hdr->msix.msix_location     = ptr;
			hdr->msix.msix_ctrl         = RREG( ptr + PCIR_MSIX_CTRL, 2 );
			hdr->msix.msix_msgnum       = (hdr->msix.msix_ctrl &
			                                  PCIM_MSIXCTRL_TABLE_SIZE) + 1;

			val = RREG( ptr + PCIR_MSIX_TABLE, 4 );
			hdr->msix.msix_table_bar    = val & PCIM_MSIX_BIR_MASK;
			hdr->msix.msix_table_offset = val & ~PCIM_MSIX_BIR_MASK;

			val = RREG( ptr + PCIR_MSIX_PBA, 4 );
			hdr->msix.msix_pba_bar      = val & PCIM_MSIX_BIR_MASK;
			hdr->msix.msix_pba_offset   = val & ~PCIM_MSIX_BIR_MASK;

			break;

		/* Bridge vendor/device ID */
		case PCIY_SUBVENDOR:
			if (hdr->hdr_type == PCIM_HDRTYPE_BRIDGE) {
				val = RREG( ptr + PCIR_SUBVENDCAP_ID, 4 );
				hdr->sub_vendor = val & 0xffff;
				hdr->sub_device = val >> 16;
			}
			break;

		}
	}
}


void
pcicfg_hdr_read(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	pcicfg_hdr_t *	hdr
)
{
	hdr->bus		= bus;
	hdr->slot		= slot;
	hdr->func		= func;
	hdr->vendor_id		= RREG( PCIR_VENDOR,    2 );
	hdr->device_id		= RREG( PCIR_DEVICE,    2 );
	hdr->command_reg	= RREG( PCIR_COMMAND,   2 );
	hdr->status_reg		= RREG( PCIR_STATUS,    2 );
	hdr->base_class		= RREG( PCIR_CLASS,     1 );
	hdr->sub_class		= RREG( PCIR_SUBCLASS,  1 );
	hdr->prog_iface		= RREG( PCIR_PROGIF,    1 );
	hdr->rev_id		= RREG( PCIR_REVID,     1 );
	hdr->hdr_type		= RREG( PCIR_HDRTYPE,   1 );
	hdr->cache_line_sz	= RREG( PCIR_CACHELNSZ, 1 );
	hdr->latency_timer	= RREG( PCIR_LATTIMER,  1 );
	hdr->interrupt_pin	= RREG( PCIR_INTPIN,    1 );
	hdr->interrupt_line	= RREG( PCIR_INTLINE,   1 );
	hdr->min_grant		= RREG( PCIR_MINGNT,    1 );
	hdr->max_latency	= RREG( PCIR_MAXLAT,    1 );
	hdr->sub_vendor		= RREG( PCIR_SUBVEND_0, 2 );
	hdr->bar[0]		= RREG( PCIR_BAR(0),    4 );
	hdr->bar[1]		= RREG( PCIR_BAR(1),    4 );
	hdr->bar[2]		= RREG( PCIR_BAR(2),    4 );
	hdr->bar[3]		= RREG( PCIR_BAR(3),    4 );
	hdr->bar[4]		= RREG( PCIR_BAR(4),    4 );
	hdr->bar[5]		= RREG( PCIR_BAR(5),    4 );
	hdr->bios		= RREG( PCIR_BIOS,      4 );

	hdr->is_multi_func	= (hdr->hdr_type & PCIM_MFDEV);
	hdr->hdr_type		&= ~PCIM_MFDEV;

	pcicfg_fixancient(hdr);
	pcicfg_hdrtypedata(bus, slot, func, hdr);

	if (hdr->status_reg & PCIM_STATUS_CAPPRESENT)
		pcicfg_read_extcap(bus, slot, func, hdr);
}


void
pcicfg_hdr1_read(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	pcicfg_hdr1_t *	hdr1
)
{
	pci_mem_addr_t base_l, base_h;		/* Base Low and High */
	pci_mem_addr_t limit_l, limit_h;
	pci_io_addr_t  io_base_l, io_base_h;
	pci_io_addr_t  io_limit_l, io_limit_h;

	/*
         * Each PCI bridge has an associated primary bus (the upstream bus
         * closer to the host processor), a secondary bus (the downstream
         * bus farther away from the host processor), and a subordinate bus
         * (the highest bus number of any bus downstream from the bridge).
         */
	hdr1->primary_bus_num     = RREG( PCIR_PRIBUS_1, 1 );
	hdr1->secondary_bus_num   = RREG( PCIR_SECBUS_1, 1 );
	hdr1->subordinate_bus_num = RREG( PCIR_SUBBUS_1, 1 );


	/*
	 * The Memory Base and Memory Limit registers define the bridge's
	 * memory window in the PCI memory address space.  The bridge will
	 * forward PCI memory transactions targeting this window onto its
	 * subordinate buses.  Both registers store bits 31:20 of the PCI
	 * memory address they represent.  For Memory Base, bits 19:0 are
	 * defined by the PCI spec to be zero, meaning the window is aligned
	 * to a 1 MB boundary.  For Memory Limit, bits 19:0 are defined to
	 * be 0xFFFFF.
	 */
	base_l  = RREG( PCIR_MEMBASE_1,  2 );
	limit_l = RREG( PCIR_MEMLIMIT_1, 2 );

	hdr1->mem_base  = (base_l  << 16) & 0xFFF00000;
	hdr1->mem_limit = (limit_l << 16) | 0x000FFFFF;


	/*
 	 * The Prefetchable Memory Base and Prefetchable Memory Limit
 	 * registers define the bridge's prefetchable memory window in the
 	 * PCI memory address space.  The window is similar to the normal
 	 * memory window described above except that the window can use
 	 * 64-bit addressing.  If 64-bit addressing is being used, bit 0
 	 * of both of these registers is set to 1 and the upper 32-bits
 	 * of each address is stored in the Prefetchable Base Upper 32 Bits
 	 * and Prefetchable Limit Upper 32 Bits registers, respectively.
 	 * As with the normal memory window, bits 19:0 are 0 for the base
 	 * address and 0xFFFFF for the limit address.
 	 */
	base_l  = RREG( PCIR_PMBASEL_1,  2 );
	base_h  = RREG( PCIR_PMBASEH_1,  4 );
	limit_l = RREG( PCIR_PMLIMITL_1, 2 );
	limit_h = RREG( PCIR_PMLIMITH_1, 4 );

	hdr1->mem_pfetch_base  = (base_l  << 16) & 0xFFF00000;
	hdr1->mem_pfetch_limit = (limit_l << 16) | 0x000FFFFF;

	/* OR in upper 32-bits if we're using 64-bit PCI memory addressing */
	if (base_l & 0x1)  hdr1->mem_pfetch_base  |= (base_h  << 32);
	if (limit_l & 0x1) hdr1->mem_pfetch_limit |= (limit_h << 32);


	/* 
	 * The I/O Base and I/O Limit registers define the bridge's I/O
	 * address window in the PCI I/O address space.  The bridge will
	 * forward PCI I/O transactions targeting this window onto its
	 * subordinate busses.  Both registers store bits 15:12 of their
	 * corresponding addresses.  Per the PCI spec, bits 11:0 of the
	 * base address are always 0, meaning the window is aligned on
	 * a 4K boundary.  For the limit address, bits 11:0 are always
	 * 0xFFF.
	 *
	 * PCI supports a 32-bit I/O address space (but note that x86
	 * only supports a 16-bit I/O address space).  If the bridge
	 * uses 32-bit I/O addressing, bit 0 of these registers is
	 * set to 1.  The upper 16 bits of each address is then defined
	 * in the I/O Base upper 16 Bits and I/O Limit Upper 16 Bits
	 * registers, respectively.
	 */
	io_base_l  = RREG( PCIR_IOBASEL_1,  1 );
	io_base_h  = RREG( PCIR_IOBASEH_1,  2 );
	io_limit_l = RREG( PCIR_IOLIMITL_1, 1 );
	io_limit_h = RREG( PCIR_IOLIMITH_1, 2 );

	hdr1->io_base  = (io_base_l  << 8) & 0xF000;
	hdr1->io_limit = (io_limit_l << 8) | 0x0FFF;

	/* OR in upper 16-bits if we're using 32-bit PCI I/O addressing */
	if (io_base_l & 0x1)  hdr1->io_base  |= (io_base_h  << 16);
	if (io_limit_l & 0x1) hdr1->io_limit |= (io_limit_h << 16);

	/* Read in other important PCI-to-PCI bridge specific info */
	hdr1->sec_latency_timer = RREG( PCIR_SECLAT_1,    1 );
	hdr1->sec_status_reg    = RREG( PCIR_SECSTAT_1,   2 );
	hdr1->bridge_ctrl_reg   = RREG( PCIR_BRIDGECTL_1, 2 );
}


/** Decodes hdr->bar[index], stores result in bar output. */
int
pcicfg_bar_decode(
	pcicfg_hdr_t *	hdr,
	unsigned int	index,
	pci_bar_t *	bar
)
{
	uint32_t bar1, bar2;
	uint8_t mem = 0;
	uint8_t type = 0;
	uint8_t prefetch = 0;
	uint64_t address;
	uint64_t mask = 0;
	uint64_t size = 0;

	bar1 = hdr->bar[index];

	// Figure out the type of the BAR, 0 = Memory, 1 = I/O
	mem = (bar1 & 0x1);

	if (mem == PCIM_BAR_MEM_SPACE) {
		// Memory BAR, has type and prefetch defined
		type     = ((bar1 >> 1) & 0x3);
		prefetch = ((bar1 >> 3) & 0x1);
		address  = (bar1 & 0xFFFFFFF0);

		// 64-bit BARs (type 2) use two indicies
		if (type == 2) {
			bar2 = hdr->bar[index + 1];
			address |= ((uint64_t)bar2 << 32);
		}
			

		/* Detect BAR size */
		/* We are going to assume that no BAR is larger than 4GB, so we 
		 *  only need to check the lower 32 bits
		 */
		pcicfg_write(hdr->bus, hdr->slot, hdr->func, 
			     PCIR_BAR(index), 4, 0xffffffff);
		
		mask = pcicfg_read(hdr->bus, hdr->slot, hdr->func,
				   PCIR_BAR(index), 4);
		mask &= 0xfffffff0;
		
		size = ((uint32_t)~mask) + 1;

		pcicfg_write(hdr->bus, hdr->slot, hdr->func, 
			     PCIR_BAR(index), 4, (uint32_t)address);

	} else {
		// I/O BAR, only address is defined
		address  = (bar1 & 0xFFFFFFFC);

		/* Detect BAR size */
		pcicfg_write(hdr->bus, hdr->slot, hdr->func, 
			     PCIR_BAR(index), 4, 0xffffffff);
		
		mask = pcicfg_read(hdr->bus, hdr->slot, hdr->func,
				   PCIR_BAR(index), 4);

		mask &= 0xfffc;
		size = ((uint16_t)~mask) + 1;

		pcicfg_write(hdr->bus, hdr->slot, hdr->func, 
			     PCIR_BAR(index), 4, (uint32_t)address);		
	}

	if (bar) {
		bar->index    = (uint8_t) index;
		bar->address  = address;
		bar->size     = size;
		bar->mem      = mem;
		bar->type     = type;
		bar->prefetch = prefetch;
	}

	return 0;
}


/* Decodes Expansion ROM bar, stores result in bar output. */
int
pcicfg_exp_rom_decode(
	pcicfg_hdr_t * hdr,
	pci_exp_rom_bar_t * bar
) {
        uint32_t exp_rom = 0;
        uint64_t mask = 0;
        uint64_t size = 0;

        exp_rom = hdr->bios;

        pcicfg_write(hdr->bus, hdr->slot, hdr->func, 
                PCIR_BIOS, 4, 0xffffffff & PCIM_BIOS_ADDR_MASK);

        mask = pcicfg_read(hdr->bus, hdr->slot, hdr->func,
                PCIR_BIOS, 4);

        mask &= PCIM_BIOS_ADDR_MASK;

        size = ((uint32_t)~mask) + 1;

        pcicfg_write(hdr->bus, hdr->slot, hdr->func, 
                PCIR_BIOS, 4, (uint32_t)exp_rom);

        if (bar) {
                bar->address  = exp_rom & PCIM_BIOS_ADDR_MASK;
                bar->size     = size;
                bar->enable     = exp_rom & PCIM_BIOS_ENABLE;
        }

        return 0;
}

#undef RCFG
#undef WCFG
