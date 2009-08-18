/*
 *	Low-Level PCI Access for i386 machines
 *
 * Copyright 1993, 1994 Drew Eckhardt
 *      Visionary Computing
 *      (Unix and Linux consulting and custom programming)
 *      Drew@Colorado.EDU
 *      +1 (303) 786-7975
 *
 * Drew's work was sponsored by:
 *	iX Multiuser Multitasking Magazine
 *	Hannover, Germany
 *	hm@ix.de
 *
 * Copyright 1997--2000 Martin Mares <mj@ucw.cz>
 *
 * For more information, please consult the following manuals (look at
 * http://www.pcisig.com/ for how to get them):
 *
 * PCI BIOS Specification
 * PCI Local Bus Specification
 * PCI to PCI Bridge Specification
 * PCI System Design Guide
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/errno.h>
#include <linux/bootmem.h>

#include <asm/pat.h>

#include "pci.h"

static int
skip_isa_ioresource_align(struct pci_dev *dev) {

	if ((pci_probe & PCI_CAN_SKIP_ISA_ALIGN) &&
	    !(dev->bus->bridge_ctl & PCI_BRIDGE_CTL_ISA))
		return 1;
	return 0;
}

/*
 * We need to avoid collisions with `mirrored' VGA ports
 * and other strange ISA hardware, so we always want the
 * addresses to be allocated in the 0x000-0x0ff region
 * modulo 0x400.
 *
 * Why? Because some silly external IO cards only decode
 * the low 10 bits of the IO address. The 0x00-0xff region
 * is reserved for motherboard devices that decode all 16
 * bits, so it's ok to allocate at, say, 0x2800-0x28ff,
 * but we want to try to avoid allocating at 0x2900-0x2bff
 * which might have be mirrored at 0x0100-0x03ff..
 */
void
pcibios_align_resource(void *data, struct resource *res,
			resource_size_t size, resource_size_t align)
{
	struct pci_dev *dev = data;

	if (res->flags & IORESOURCE_IO) {
		resource_size_t start = res->start;

		if (skip_isa_ioresource_align(dev))
			return;
		if (start & 0x300) {
			start = (start + 0x3ff) & ~0x3ff;
			res->start = start;
		}
	}
}
EXPORT_SYMBOL(pcibios_align_resource);

/*
 *  Handle resources of PCI devices.  If the world were perfect, we could
 *  just allocate all the resource regions and do nothing more.  It isn't.
 *  On the other hand, we cannot just re-allocate all devices, as it would
 *  require us to know lots of host bridge internals.  So we attempt to
 *  keep as much of the original configuration as possible, but tweak it
 *  when it's found to be wrong.
 *
 *  Known BIOS problems we have to work around:
 *	- I/O or memory regions not configured
 *	- regions configured, but not enabled in the command register
 *	- bogus I/O addresses above 64K used
 *	- expansion ROMs left enabled (this may sound harmless, but given
 *	  the fact the PCI specs explicitly allow address decoders to be
 *	  shared between expansion ROMs and other resource regions, it's
 *	  at least dangerous)
 *
 *  Our solution:
 *	(1) Allocate resources for all buses behind PCI-to-PCI bridges.
 *	    This gives us fixed barriers on where we can allocate.
 *	(2) Allocate resources for all enabled devices.  If there is
 *	    a collision, just mark the resource as unallocated. Also
 *	    disable expansion ROMs during this step.
 *	(3) Try to allocate resources for disabled devices.  If the
 *	    resources were assigned correctly, everything goes well,
 *	    if they weren't, they won't disturb allocation of other
 *	    resources.
 *	(4) Assign new addresses to resources which were either
 *	    not configured at all or misconfigured.  If explicitly
 *	    requested by the user, configure expansion ROM address
 *	    as well.
 */

static void __init pcibios_allocate_bus_resources(struct list_head *bus_list)
{
	struct pci_bus *bus;
	struct pci_dev *dev;
	int idx;
	struct resource *r, *pr;

	/* Depth-First Search on bus tree */
	list_for_each_entry(bus, bus_list, node) {
		if ((dev = bus->self)) {
			for (idx = PCI_BRIDGE_RESOURCES;
			    idx < PCI_NUM_RESOURCES; idx++) {
				r = &dev->resource[idx];
				if (!r->flags)
					continue;
				pr = pci_find_parent_resource(dev, r);
				if (!r->start || !pr ||
				    request_resource(pr, r) < 0) {
					dev_err(&dev->dev, "BAR %d: can't allocate resource\n", idx);
					/*
					 * Something is wrong with the region.
					 * Invalidate the resource to prevent
					 * child resource allocations in this
					 * range.
					 */
					r->flags = 0;
				}
			}
		}
		pcibios_allocate_bus_resources(&bus->children);
	}
}

static void __init pcibios_allocate_resources(int pass)
{
	struct pci_dev *dev = NULL;
	int idx, disabled;
	u16 command;
	struct resource *r, *pr;

	for_each_pci_dev(dev) {
		pci_read_config_word(dev, PCI_COMMAND, &command);
		for (idx = 0; idx < PCI_ROM_RESOURCE; idx++) {
			r = &dev->resource[idx];
			if (r->parent)		/* Already allocated */
				continue;
			if (!r->start)		/* Address not assigned at all */
				continue;
			if (r->flags & IORESOURCE_IO)
				disabled = !(command & PCI_COMMAND_IO);
			else
				disabled = !(command & PCI_COMMAND_MEMORY);
			if (pass == disabled) {
				dev_dbg(&dev->dev, "resource %#08llx-%#08llx (f=%lx, d=%d, p=%d)\n",
					(unsigned long long) r->start,
					(unsigned long long) r->end,
					r->flags, disabled, pass);
				pr = pci_find_parent_resource(dev, r);
				if (!pr || request_resource(pr, r) < 0) {
					dev_err(&dev->dev, "BAR %d: can't allocate resource\n", idx);
					/* We'll assign a new address later */
					r->end -= r->start;
					r->start = 0;
				}
			}
		}
		if (!pass) {
			r = &dev->resource[PCI_ROM_RESOURCE];
			if (r->flags & IORESOURCE_ROM_ENABLE) {
				/* Turn the ROM off, leave the resource region,
				 * but keep it unregistered. */
				u32 reg;
				dev_dbg(&dev->dev, "disabling ROM\n");
				r->flags &= ~IORESOURCE_ROM_ENABLE;
				pci_read_config_dword(dev,
						dev->rom_base_reg, &reg);
				pci_write_config_dword(dev, dev->rom_base_reg,
						reg & ~PCI_ROM_ADDRESS_ENABLE);
			}
		}
	}
}

static int __init pcibios_assign_resources(void)
{
	struct pci_dev *dev = NULL;
	struct resource *r, *pr;

	if (!(pci_probe & PCI_ASSIGN_ROMS)) {
		/*
		 * Try to use BIOS settings for ROMs, otherwise let
		 * pci_assign_unassigned_resources() allocate the new
		 * addresses.
		 */
		for_each_pci_dev(dev) {
			r = &dev->resource[PCI_ROM_RESOURCE];
			if (!r->flags || !r->start)
				continue;
			pr = pci_find_parent_resource(dev, r);
			if (!pr || request_resource(pr, r) < 0) {
				r->end -= r->start;
				r->start = 0;
			}
		}
	}

	pci_assign_unassigned_resources();

	return 0;
}

void __init pcibios_resource_survey(void)
{
	DBG("PCI: Allocating resources\n");
	pcibios_allocate_bus_resources(&pci_root_buses);
	pcibios_allocate_resources(0);
	pcibios_allocate_resources(1);
}

/**
 * called in fs_initcall (one below subsys_initcall),
 * give a chance for motherboard reserve resources
 */
fs_initcall(pcibios_assign_resources);

/*
 *  If we set up a device for bus mastering, we need to check the latency
 *  timer as certain crappy BIOSes forget to set it properly.
 */
unsigned int pcibios_max_latency = 255;

void pcibios_set_master(struct pci_dev *dev)
{
	u8 lat;
	pci_read_config_byte(dev, PCI_LATENCY_TIMER, &lat);
	if (lat < 16)
		lat = (64 <= pcibios_max_latency) ? 64 : pcibios_max_latency;
	else if (lat > pcibios_max_latency)
		lat = pcibios_max_latency;
	else
		return;
	dev_printk(KERN_DEBUG, &dev->dev, "setting latency timer to %d\n", lat);
	pci_write_config_byte(dev, PCI_LATENCY_TIMER, lat);
}

static void pci_unmap_page_range(struct vm_area_struct *vma)
{
	u64 addr = (u64)vma->vm_pgoff << PAGE_SHIFT;
	free_memtype(addr, addr + vma->vm_end - vma->vm_start);
}

static void pci_track_mmap_page_range(struct vm_area_struct *vma)
{
	u64 addr = (u64)vma->vm_pgoff << PAGE_SHIFT;
	unsigned long flags = pgprot_val(vma->vm_page_prot)
						& _PAGE_CACHE_MASK;

	reserve_memtype(addr, addr + vma->vm_end - vma->vm_start, flags, NULL);
}

static struct vm_operations_struct pci_mmap_ops = {
	.open  = pci_track_mmap_page_range,
	.close = pci_unmap_page_range,
	.access = generic_access_phys,
};

int pci_mmap_page_range(struct pci_dev *dev, struct vm_area_struct *vma,
			enum pci_mmap_state mmap_state, int write_combine)
{
	unsigned long prot;
	u64 addr = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long len = vma->vm_end - vma->vm_start;
	unsigned long flags;
	unsigned long new_flags;
	int retval;

	/* I/O space cannot be accessed via normal processor loads and
	 * stores on this platform.
	 */
	if (mmap_state == pci_mmap_io)
		return -EINVAL;

	prot = pgprot_val(vma->vm_page_prot);
	if (pat_enabled && write_combine)
		prot |= _PAGE_CACHE_WC;
	else if (pat_enabled || boot_cpu_data.arch.x86_family > 3)
		/*
		 * ioremap() and ioremap_nocache() defaults to UC MINUS for now.
		 * To avoid attribute conflicts, request UC MINUS here
		 * aswell.
		 */
		prot |= _PAGE_CACHE_UC_MINUS;

	vma->vm_page_prot = __pgprot(prot);

	flags = pgprot_val(vma->vm_page_prot) & _PAGE_CACHE_MASK;
	retval = reserve_memtype(addr, addr + len, flags, &new_flags);
	if (retval)
		return retval;

	if (flags != new_flags) {
		/*
		 * Do not fallback to certain memory types with certain
		 * requested type:
		 * - request is uncached, return cannot be write-back
		 * - request is uncached, return cannot be write-combine
		 * - request is write-combine, return cannot be write-back
		 */
		if ((flags == _PAGE_CACHE_UC_MINUS &&
		     (new_flags == _PAGE_CACHE_WB)) ||
		    (flags == _PAGE_CACHE_WC &&
		     new_flags == _PAGE_CACHE_WB)) {
			free_memtype(addr, addr+len);
			return -EINVAL;
		}
		flags = new_flags;
		vma->vm_page_prot = __pgprot(
			(pgprot_val(vma->vm_page_prot) & ~_PAGE_CACHE_MASK) |
			flags);
	}

	if (((vma->vm_pgoff < end_pfn) ||
	     (vma->vm_pgoff >= (1UL<<(32 - PAGE_SHIFT)) &&
	      vma->vm_pgoff < end_pfn)) &&
	    ioremap_change_attr((unsigned long)__va(addr), len, flags)) {
		free_memtype(addr, addr + len);
		return -EINVAL;
	}

	if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff,
			       vma->vm_end - vma->vm_start,
			       vma->vm_page_prot))
		return -EAGAIN;

	vma->vm_ops = &pci_mmap_ops;

	return 0;
}
