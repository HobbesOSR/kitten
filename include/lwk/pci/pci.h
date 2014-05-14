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


struct msix_entry {
  u16 vector;
  u16 entry;
}__attribute__((packed));

struct msi_msg {
    u32 address_hi;
    union {
      uint32_t address_lo;
      struct {
        uint32_t    rsvd1         : 2;
        uint32_t    dest_mode     : 1;
        uint32_t    redirect_hint : 1;
        uint32_t    rsvd2         : 8;
        uint32_t    dest          : 8; 
        uint32_t    rsvd3         : 12; 
        } __attribute__((packed));
    } __attribute__((packed));
    union {
      uint32_t data;
      struct {
        uint32_t    vector        : 8;
        uint32_t    delivery_mode : 3;
        uint32_t    rsvd4         : 3;
        uint32_t    level         : 1;
        uint32_t    trigger_mode  : 1;
        uint32_t    rsvd5         : 16;
        } __attribute__((packed));
    } __attribute__((packed));
} __attribute__((packed));

/** Initializes the PCI subsystem, called once at boot. */
void init_pci(void);


/** Searches for a PCI device matching the input vendor ID and device ID. */
pci_dev_t *pci_lookup_device(uint16_t vendor_id, uint16_t device_id);

/** Searches for a PCI device matching the input bus and devfn. */
pci_dev_t *pci_get_dev_bus_and_slot(uint32_t bus, uint32_t devfn);


/** Creates a human-readable description of a PCI device. */
void pci_describe_device(pci_dev_t *dev, size_t len, char *buf);


/** Reads a value from a PCI device's config space header. */
uint32_t pci_read(pci_dev_t *dev, unsigned int reg, unsigned int width);


/** Writes a value to a PCI device's config space header. */
void pci_write(pci_dev_t *dev, unsigned int reg, unsigned int width, uint32_t value);

void pci_intx_enable(pci_dev_t *dev);
void pci_intx_disable(pci_dev_t *dev);
int pci_msi_setup(pci_dev_t *dev, u8 vector);
void pci_msi_enable(pci_dev_t *dev);
void pci_msi_disable(pci_dev_t *dev);
int pci_msix_setup(pci_dev_t *dev, struct msix_entry * entries, u32 num_entries);
void pci_msix_enable(pci_dev_t * dev);
void pci_msix_disable(pci_dev_t * dev);

void compose_msi_msg(struct msi_msg *msg, unsigned int dest, u8 vector);
void read_msi_msg(pci_dev_t * dev, struct msi_msg * msg);
void write_msi_msg(pci_dev_t * dev, struct msi_msg * msg);
void set_msi_msg_nr(pci_dev_t * dev, unsigned int n);

#endif
