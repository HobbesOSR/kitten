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
typedef struct pci_bus {
	struct pci_bus *	parent_bus;	//!< pointer to this bus's parent bus
	struct list_head	child_buses;	//!< list of buses directly downstream from this one
	struct list_head	next_bus;	//!< linkage for parent bus's list of child busses

	unsigned int		bus;		//!< this bus's bus number
	struct list_head	devices;	//!< list of devices on this bus 

	struct pci_dev *	self;		//!< pointer to upstream bridge device to this bus
	struct pcicfg_hdr1	bridge_info;	//!< bridge specific config hdr info not described by self->cfg
} pci_bus_t;


/** Describes a PCI device.
 *
 * One of these structures exists for each PCI device in the system.
 */
typedef struct pci_dev {
	struct list_head	siblings;	//!< linkage for list of devices on the same bus
	struct list_head	next;		//!< linkage for list of all devices in system

	uint8_t			slot;		//!< config space slot number of the device
	uint8_t			func;		//!< config space function number of the device

	struct pci_bus *	parent_bus;	//!< pointer to this device's parent bus
	struct pcicfg_hdr	cfg;		//!< a copy of the device's config header

	struct pci_driver *	driver;		//!< the driver associated with this device
} pci_dev_t;


/** Initializes the PCI subsystem, called once at boot. */
void init_pci(void);


/** Searches for a PCI device matching the input vendor ID and device ID. */
pci_dev_t *pci_lookup_device(uint16_t vendor_id, uint16_t device_id);


/** Creates a human-readable description of a PCI device. */
void pci_describe_device(pci_dev_t *dev, size_t len, char *buf);


/** Reads a value from a PCI device's config space header. */
uint32_t pci_read(pci_dev_t *dev, unsigned int reg, unsigned int width);


/** Writes a value to a PCI device's config space header. */
void pci_write(pci_dev_t *dev, unsigned int reg, unsigned int width, uint32_t value);


#endif
