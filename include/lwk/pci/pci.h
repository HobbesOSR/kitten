/* Copyright (c) 2009, Sandia National Laboratories */

#ifndef _LWK_PCI_H
#define _LWK_PCI_H

#include <lwk/list.h>
#include <lwk/pci/pcicfg.h>


/** Describes a PCI bus.
 *
 * One of these structures exists for each PCI bus in the system.  It links to
 * all of the PCI devices on the bus, each represented by a 'struct pci_dev'.
 */
struct pci_bus {
	struct pci_bus *	parent_bus;	//!< pointer to this bus's parent bus
	struct list_head	child_buses;	//!< list of buses directly downstream from this one
	struct list_head	next_bus;	//!< linkage for parent bus's list of child busses

	unsigned int		bus;		//!< this bus's bus number
	struct list_head	devices;	//!< list of devices on this bus 

	struct pci_dev *	self;		//!< pointer to upstream bridge device to this bus
	struct pcicfg_hdr1	bridge_info;	//!< bridge specific config hdr info not described by self->cfg
};


/** Describes a PCI device.
 *
 * One of these structures exists for each PCI device in the system.
 */
struct pci_dev {
	struct list_head	siblings;	//!< linkage for list of devices on the same bus
	struct list_head	next;		//!< linkage for list of all devices in system

	uint8_t			slot;		//!< config space slot number of the device
	uint8_t			func;		//!< config space function number of the device

	struct pci_bus *	parent_bus;	//!< pointer to this device's parent bus
	struct pcicfg_hdr	cfg;		//!< a copy of the device's config header

	struct pci_driver *	driver;		//!< the driver associated with this device
};


void
pci_describe_device(
	struct pci_dev *	dev,
	size_t			len,
	char *			buf
);


void init_pci(void);


#endif
