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


/**
 * Describes a device that can be handled by a PCI driver.
 *
 * When a PCI driver registers with the LWK, it provides an array of
 * pci_dev_id_t structures specifying which PCI devices it can handle.
 */
typedef struct pci_dev_id {
	uint32_t		vendor_id;
	uint32_t		device_id;
	uint32_t		subvendor_id;
	uint32_t		subdevice_id;
	uint32_t		class_mask;
	uintptr_t		driver_data;
} pci_dev_id_t;


typedef unsigned int __bitwise pci_ers_result_t;
typedef unsigned int __bitwise pci_channel_state_t;

typedef struct pm_message {
        int event;
} pm_message_t;



enum pci_channel_state {
        /* I/O channel is in normal state */
        pci_channel_io_normal = (__force pci_channel_state_t) 1,

        /* I/O to channel is blocked */
        pci_channel_io_frozen = (__force pci_channel_state_t) 2,

        /* PCI card is dead */
        pci_channel_io_perm_failure = (__force pci_channel_state_t) 3,
};

enum pci_ers_result {
        /* no result/none/not supported in device driver */
        PCI_ERS_RESULT_NONE = (__force pci_ers_result_t) 1,

        /* Device driver can recover without slot reset */
        PCI_ERS_RESULT_CAN_RECOVER = (__force pci_ers_result_t) 2,

        /* Device driver wants slot to be reset. */
        PCI_ERS_RESULT_NEED_RESET = (__force pci_ers_result_t) 3,

        /* Device has completely failed, is unrecoverable */
        PCI_ERS_RESULT_DISCONNECT = (__force pci_ers_result_t) 4,

        /* Device driver is fully recovered and operational */
        PCI_ERS_RESULT_RECOVERED = (__force pci_ers_result_t) 5,
};

/* PCI bus error event callbacks */
struct pci_error_handlers {
        /* PCI bus error detected on this device */
        pci_ers_result_t (*error_detected)(struct pci_dev *dev,
                        enum pci_channel_state error);

        /* MMIO has been re-enabled, but not DMA */
        pci_ers_result_t (*mmio_enabled)(struct pci_dev *dev);

        /* PCI Express link has been reset */
        pci_ers_result_t (*link_reset)(struct pci_dev *dev);

        /* PCI slot has been reset */
        pci_ers_result_t (*slot_reset)(struct pci_dev *dev);

        /* Device driver may resume normal operations */
        void (*resume)(struct pci_dev *dev);
};


/** Describes a PCI driver.
 *
 * One of these structures exist for each PCI device driver that has
 * registered itself.
 */
typedef struct pci_driver {
	struct list_head	next;		//!< linkage for the global list of PCI drivers
	char *			name;		//!< human readable name of the device
	const pci_dev_id_t *	id_table;	//!< array of PCI device IDs the driver can handle
	int  (*probe)   (pci_dev_t *dev, const pci_dev_id_t *id);
	void (*remove)  (struct pci_dev * dev);
        int  (*suspend) (struct pci_dev * dev, pm_message_t state);      /* Device suspended */
        int  (*resume)  (struct pci_dev * dev);                          /* Device woken up */

        struct pci_error_handlers * err_handler;
} pci_driver_t;



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


#define PCI_DEVFN(slot, func)   ((((slot) & 0x1f) << 3) | ((func) & 0x07))
#define PCI_SLOT(devfn)         (((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)         ((devfn) & 0x07)

/** Initializes the PCI subsystem, called once at boot. */
void init_pci(void);


/** Registers a PCI driver with the LWK. */
int pci_register_driver(pci_driver_t *driver);

void pci_unregister_driver(pci_driver_t * driver);

/** Finds a PCI driver suitable for the PCI device passed in. */
pci_driver_t *pci_find_driver(const pci_dev_t *dev, const pci_dev_id_t **idp);


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



void pci_dma_enable     (pci_dev_t * dev);
void pci_dma_disable    (pci_dev_t * dev); 
void pci_mmio_enable    (pci_dev_t * dev);
void pci_mmio_disable   (pci_dev_t * dev);
void pci_ioport_enable  (pci_dev_t * dev);
void pci_ioport_disable (pci_dev_t * dev);
void pci_intx_enable    (pci_dev_t * dev);
void pci_intx_disable   (pci_dev_t * dev);

int  pci_msi_setup      (pci_dev_t * dev, u8 vector);
void pci_msi_enable     (pci_dev_t * dev);
void pci_msi_disable    (pci_dev_t * dev);
int  pci_msix_setup     (pci_dev_t * dev, struct msix_entry * entries, u32 num_entries);
void pci_msix_enable    (pci_dev_t * dev);
void pci_msix_disable   (pci_dev_t * dev);

void compose_msi_msg (struct msi_msg * msg, unsigned int dest, u8 vector);
void set_msi_msg_nr  (pci_dev_t * dev, unsigned int n);
void read_msi_msg    (pci_dev_t * dev, struct msi_msg * msg);
void write_msi_msg   (pci_dev_t * dev, struct msi_msg * msg);

#endif
