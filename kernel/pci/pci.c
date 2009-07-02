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
#include <lwk/pci/pci.h>


/** The root PCI bus.
 *
 * This is the root of the PCI bus, i.e., the one connected to the
 * Host-to-PCI bridge.  The PCI buses in the system form a tree
 * where PCI-to-PCI bridges connect upstream buses to downstream
 * buses.  Each bus is connected to exactly one upstream bus (or
 * the host in the case of the pci_root).
 */
struct pci_bus *pci_root;


/** A list of all PCI devices in the system.
 *
 * This is a global list of all PCI devices in the system.
 * PCI-to-PCI bridges are considered PCI devices, as are
 * as more traditional devices like network adapters and
 * video cards.
 */
LIST_HEAD(pci_devices);


static struct pci_dev *
alloc_pci_dev(void)
{
	struct pci_dev *new_dev = kmem_alloc(sizeof(struct pci_dev));
	if (!new_dev)
		panic("Out of memory allocating new pci_dev.");
	return new_dev;
}


static struct pci_bus *
alloc_pci_bus(void)
{
	struct pci_bus *new_bus = kmem_alloc(sizeof(struct pci_bus));
	if (!new_bus)
		panic("Out of memory allocating new pci_bus.");

	list_head_init(&new_bus->child_buses);
	list_head_init(&new_bus->devices);

	return new_bus;
}


static void
pci_scan_bus(struct pci_bus * bus)
{
	printk(KERN_INFO
		"Scanning PCI Bus %u (parent=%u, subordinate=%u)\n",
		bus->bus,
		bus->bridge_info.primary_bus_num,
		bus->bridge_info.subordinate_bus_num
	);

	/* Scan all slots on the bus */
	for (int slot = 0; slot <= PCI_SLOTMAX; slot++) {

		/* Read device's config header type */
		uint8_t hdr_type = pcicfg_read(bus->bus, slot, 0, PCIR_HDRTYPE, 1);

		/* Bail if no device in the slot */
		if ((hdr_type & PCIM_HDRTYPE) > PCI_MAXHDRTYPE)
			continue;

		/* Determine the device's maximum function index */
		int max_func = (hdr_type & PCIM_MFDEV) ? PCI_FUNCMAX : 0;

		/* Scan all of the device's functions */
		for (int func = 0; func <= max_func; func++) {

			/* Read the device ID and vendor ID all at once */
			uint32_t dev_vendor = pcicfg_read(bus->bus, slot, func,
			                                  PCIR_DEVVENDOR, 4);

			/* Bail if device doesn't implement the function */
			if (dev_vendor == 0xFFFFFFFF)
				continue;

			/* Each funcion is represented by a separate pci_dev */
			struct pci_dev *new_dev = alloc_pci_dev();

			pcicfg_hdr_read(bus->bus, slot, func, &new_dev->cfg);
			new_dev->parent_bus = bus;
			new_dev->slot       = slot;
			new_dev->func       = func;
			new_dev->driver	    = NULL;

			/* Add new device onto parent bus's list of devices */
			list_add_tail(&new_dev->siblings, &bus->devices);

			/* Add new device onto global list of devices */
			list_add_tail(&new_dev->next, &pci_devices);

			/* Create human-readable string describing the device */
			char desc[80];
			pci_describe_device(new_dev, sizeof(desc), desc);

			printk(KERN_INFO
				"    Found PCI Device: %u:%u.%u %4x:%-4x %s\n",
				bus->bus,
				slot,
				func,
				(unsigned int) new_dev->cfg.vendor_id,
				(unsigned int) new_dev->cfg.device_id,
				desc
			);
		}
	}

	/* Traverse each PCI-to-PCI bridge */
	struct pci_dev *dev;
	list_for_each_entry(dev, &bus->devices, siblings) {

		/* If it isn't a PCI-to-PCI bridge, move onto the next device */
		if (dev->cfg.hdr_type != PCIM_HDRTYPE_BRIDGE)
			continue;

		/* Allocate a bus structure for the bridge's downstream bus */
		struct pci_bus *new_bus = alloc_pci_bus();

		pcicfg_hdr1_read(bus->bus, dev->slot, dev->func, &new_bus->bridge_info);
		new_bus->bus        = new_bus->bridge_info.secondary_bus_num;
		new_bus->parent_bus = bus;
		new_bus->self       = dev;

		/* Add new bus onto parent bus's list of child busses */
		list_add_tail(&new_bus->next_bus, &bus->child_buses);

		/* Recursively scan the new bus */
		pci_scan_bus(new_bus);
	}
}


void
init_pci(void)
{
	pci_root = alloc_pci_bus();

	pci_root->bus        = 0;
	pci_root->parent_bus = NULL;
	pci_root->self       = NULL;

	pci_root->bridge_info.primary_bus_num     = 0;
	pci_root->bridge_info.secondary_bus_num   = 0;
	pci_root->bridge_info.subordinate_bus_num = 0xFF;

	pci_scan_bus(pci_root);
}
