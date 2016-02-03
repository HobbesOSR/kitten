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
#include <lwk/device.h>
#include <lwk/pci/pci.h>
#include <lwk/pci/msidef.h>
#include <lwk/cpuinfo.h>
#include <lwk/resource.h>
#include <lwk/spinlock.h>
#include <arch/io.h>


/** Lock that protects the entire PCI subsystem. */
static DEFINE_SPINLOCK(pci_lock);


/** The root PCI bus.
 *
 * This is the root of the PCI bus, i.e., the one connected to the
 * Host-to-PCI bridge.  The PCI buses in the system form a tree
 * where PCI-to-PCI bridges connect upstream buses to downstream
 * buses.  Each bus is connected to exactly one upstream bus (or
 * the host in the case of the pci_root).
 */
pci_bus_t *pci_root;


/** A list of all PCI devices in the system.
 *
 * This is a global list of all PCI devices in the system.
 * PCI-to-PCI bridges are considered PCI devices, as are
 * as more traditional devices like network adapters and
 * video cards.
 */
LIST_HEAD(pci_devices);


/** A list of all PCI device drivers.
 *
 * This is a global list of all PCI device drivers that
 * have registered with the LWK. When a new PCI device is
 * discovered, this list is searched to see if there is
 * a suitable driver available.
 */
static LIST_HEAD(pci_drivers);



extern struct device  dev_root;



static pci_dev_t *
alloc_pci_dev(void)
{
	pci_dev_t *new_dev = kmem_alloc(sizeof(pci_dev_t));
	if (!new_dev)
		panic("Out of memory allocating new pci_dev.");
	return new_dev;
}


static pci_bus_t *
alloc_pci_bus(void)
{
	pci_bus_t *new_bus = kmem_alloc(sizeof(pci_bus_t));
	if (!new_bus)
		panic("Out of memory allocating new pci_bus.");

	list_head_init(&new_bus->child_buses);
	list_head_init(&new_bus->devices);

	return new_bus;
}


static void
pci_scan_bus(pci_bus_t * bus)
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
		if ((hdr_type & PCIM_HDRTYPE) > PCI_MAXHDRTYPE) {
			continue;
		}

		/* Determine the device's maximum function index */
		int max_func = (hdr_type & PCIM_MFDEV) ? PCI_FUNCMAX : 0;

		/* Scan all of the device's functions */
		for (int func = 0; func <= max_func; func++) {

			/* Read the device ID and vendor ID all at once */
			uint32_t dev_vendor = pcicfg_read(bus->bus, slot, func,
			                                  PCIR_DEVVENDOR, 4);

			/* Bail if device doesn't implement the function */
			if (dev_vendor == 0xFFFFFFFF) {
				continue;
			}

			/* Each funcion is represented by a separate pci_dev */
			pci_dev_t * new_dev = alloc_pci_dev();

			pcicfg_hdr_read(bus->bus, slot, func, &new_dev->cfg);
			new_dev->parent_bus = bus;
			new_dev->slot       = slot;
			new_dev->func       = func;

			/* Redundant fields. Needed for Linux compatibility */
			new_dev->devfn      = PCI_DEVFN(slot, func); 
			new_dev->device     = new_dev->cfg.device_id;
			new_dev->vendor     = new_dev->cfg.vendor_id;
			new_dev->revision   = new_dev->cfg.rev_id;

			new_dev->driver	    = NULL;			

			/* For now we are going to disable all legacy IRQs in Kitten */
			/* This will cause request_irq to fail when called with this as an argument */
			new_dev->irq        = (uint32_t)(-1);

			/* Add new device onto parent bus's list of devices */
			list_add_tail(&new_dev->siblings, &bus->devices);

			/* Add new device onto global list of devices */
			list_add_tail(&new_dev->next, &pci_devices);

			/* Create human-readable string describing the device */
			pci_describe_device(new_dev, sizeof(new_dev->name), new_dev->name);

			/* Initialize the device layer */
			device_init(&(new_dev->dev), NULL, &dev_root, 0, NULL, new_dev->name);

			printk(KERN_INFO
				"    Found PCI Device: %u:%u.%u %4x:%-4x %s\n",
				bus->bus,
				slot,
				func,
				(unsigned int) new_dev->cfg.vendor_id,
				(unsigned int) new_dev->cfg.device_id,
				new_dev->name
			);
		}
	}

	/* Traverse each PCI-to-PCI bridge */
	pci_dev_t * dev = NULL;
	list_for_each_entry(dev, &bus->devices, siblings) {

		/* If it isn't a PCI-to-PCI bridge, move onto the next device */
		if (dev->cfg.hdr_type != PCIM_HDRTYPE_BRIDGE) {
			continue;
		}

		/* Allocate a bus structure for the bridge's downstream bus */
		pci_bus_t * new_bus = alloc_pci_bus();

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


pci_dev_t *
pci_lookup_device(uint16_t vendor_id, uint16_t device_id)
{
	pci_dev_t *dev;

	list_for_each_entry(dev, &pci_devices, next) {
		if ((dev->cfg.vendor_id == vendor_id) && ((dev->cfg.device_id == device_id)))
			return dev;
	}

	return NULL;
}

pci_dev_t *
pci_get_dev_bus_and_slot(uint32_t bus, uint32_t devfn)
{
    pci_dev_t * dev;

    list_for_each_entry(dev, &pci_devices, next) {
        if ((dev->cfg.bus == bus) && ((PCI_DEVFN(dev->cfg.slot, dev->cfg.func) == devfn)))
            return dev;
    }

    return NULL;
}


int 
pci_find_capability(pci_dev_t * dev, int capid)
{
    return pcicfg_find_cap_offset(dev->parent_bus->bus, dev->slot, dev->func, &(dev->cfg), capid);
}


const char *
pci_name(pci_dev_t * dev)
{
    return dev->name;
}

/** Reads a value from a PCI device's config space header. */
uint32_t
pci_read(pci_dev_t *dev, unsigned int reg, unsigned int width)
{
	return pcicfg_read(dev->parent_bus->bus, dev->slot, dev->func, reg, width);
}


/** Writes a value to a PCI device's config space header. */
void
pci_write(pci_dev_t *dev, unsigned int reg, unsigned int width, uint32_t value)
{
	pcicfg_write(dev->parent_bus->bus, dev->slot, dev->func, reg, width, value);
}

void 
pci_dma_enable(pci_dev_t * dev) 
{
	u16 cmd =  pci_read(dev, PCIR_COMMAND, 2);
	pci_write(dev, PCIR_COMMAND, 2, (cmd | PCIM_CMD_BUSMASTEREN));
}


void 
pci_dma_disable(pci_dev_t * dev) 
{
	u16 cmd =  pci_read(dev, PCIR_COMMAND, 2);
	pci_write(dev, PCIR_COMMAND, 2, (cmd & ~PCIM_CMD_BUSMASTEREN));
}


void 
pci_mmio_enable(pci_dev_t * dev) 
{
	u16 cmd =  pci_read(dev, PCIR_COMMAND, 2);
	pci_write(dev, PCIR_COMMAND, 2, (cmd | PCIM_CMD_MEMEN));
}


void 
pci_mmio_disable(pci_dev_t * dev) 
{
	u16 cmd =  pci_read(dev, PCIR_COMMAND, 2);
	pci_write(dev, PCIR_COMMAND, 2, (cmd & ~PCIM_CMD_MEMEN));
}

void 
pci_ioport_enable(pci_dev_t * dev) 
{
	u16 cmd =  pci_read(dev, PCIR_COMMAND, 2);
	pci_write(dev, PCIR_COMMAND, 2, (cmd | PCIM_CMD_PORTEN));
}


void 
pci_ioport_disable(pci_dev_t * dev) 
{
	u16 cmd =  pci_read(dev, PCIR_COMMAND, 2);
	pci_write(dev, PCIR_COMMAND, 2, (cmd & ~PCIM_CMD_PORTEN));
}


void 
pci_intx_enable(pci_dev_t * dev)
{
	u16 cmd =  pci_read(dev, PCIR_COMMAND, 2);
	pci_write(dev, PCIR_COMMAND, 2, (cmd & ~PCIM_CMD_INTxDIS));
}

void 
pci_intx_disable(pci_dev_t * dev)
{
	u16 cmd =  pci_read(dev, PCIR_COMMAND, 2);
	pci_write(dev, PCIR_COMMAND, 2, (cmd | PCIM_CMD_INTxDIS));
}

void 
compose_msi_msg(struct msi_msg  *msg,
		unsigned int     dest,
		u8               vector)
{

    msg->address_hi = MSI_ADDR_BASE_HI;

    /* do not support x2apic */
    //if (x2apic_enabled())
    //    msg->address_hi |= MSI_ADDR_EXT_DEST_ID(dest);

    msg->address_lo =
        MSI_ADDR_BASE_LO |
        MSI_ADDR_DEST_MODE_PHYSICAL |
        MSI_ADDR_REDIRECTION_CPU |  /* disable lowest priority mode*/
        MSI_ADDR_DEST_ID(dest);

    msg->data =
        MSI_DATA_TRIGGER_EDGE |
        MSI_DATA_LEVEL_ASSERT |
        MSI_DATA_DELIVERY_FIXED |
        MSI_DATA_VECTOR(vector);
}

void 
read_msi_msg(pci_dev_t      * dev,
	     struct msi_msg * msg)
{
    pcicfg_hdr_t  * hdr   = &dev->cfg;
    unsigned long   pos   = hdr->msi.msi_location;
    int             is_64 = hdr->msi.msi_ctrl & PCIM_MSICTRL_64BIT;

    msg->address_lo = pci_read(dev, pos + PCIR_MSI_ADDR, 4);

    if (is_64) {
        msg->address_hi = pci_read(dev, pos + PCIR_MSI_ADDR_HIGH,  4);
        msg->data       = pci_read(dev, pos + PCIR_MSI_DATA_64BIT, 2);
    } else {
        msg->data       = pci_read(dev, pos + PCIR_MSI_DATA,       2);
    }
}


void 
write_msi_msg(pci_dev_t      * dev,
	      struct msi_msg * msg)
{
    pcicfg_hdr_t   * hdr   = &dev->cfg;
    unsigned long    pos   = hdr->msi.msi_location;
    int              is_64 = hdr->msi.msi_ctrl & PCIM_MSICTRL_64BIT;

    pci_write(dev, pos + PCIR_MSI_ADDR, 4, msg->address_lo);

    if (is_64) {
        pci_write(dev, pos + PCIR_MSI_ADDR_HIGH,  4, msg->address_hi);
        pci_write(dev, pos + PCIR_MSI_DATA_64BIT, 2, msg->data);
    } else {
        pci_write(dev, pos + PCIR_MSI_DATA,       2, msg->data);
    }
}

void 
set_msi_msg_nr(pci_dev_t    * dev, 
	       unsigned int   n)
{
    pcicfg_hdr_t  * hdr = &dev->cfg;
    unsigned long   pos = hdr->msi.msi_location;

    u16 msgctl = 0;

    hdr->msi.msi_alloc = n;

    msgctl  =  pci_read(dev, pos + PCIR_MSI_CTRL, 2);
    msgctl &= ~PCIM_MSICTRL_MME_MASK;
    msgctl |=  hdr->msi.msi_alloc << 4;

    pci_write(dev, pos + PCIR_MSI_CTRL, 2, msgctl);
}

static void 
pci_set_msi(pci_dev_t * dev, 
	    int         enable)
{
    pcicfg_hdr_t   * hdr = &dev->cfg;
    unsigned long    pos = hdr->msi.msi_location;

    u16 control = 0;
    u16 new     = 0;

    control = pci_read(dev, pos + PCIR_MSI_CTRL, 2);
    if (enable) {
        new = control |  PCIM_MSICTRL_MSI_ENABLE;
    } else {
        new = control & ~PCIM_MSICTRL_MSI_ENABLE;
    }

    if (new != control) {
        pci_write(dev, pos + PCIR_MSI_CTRL, 2, new);
    } else {
        printk("Warning: MSI already %s!\n",
                enable ? "enabled" : "disabled");
    }
}

/* Currently only support ONE vector  */
int 
pci_msi_setup(pci_dev_t * dev,
	      u8          vector)
{
    extern struct cpuinfo cpu_info[NR_CPUS];
    struct msi_msg msg;

    int dest = cpu_info[0].arch.apic_id;

    set_msi_msg_nr(dev, 1);

    compose_msi_msg(&msg, dest, vector);
    write_msi_msg(dev, &msg);

    pci_intx_disable(dev);
    pci_set_msi(dev, 1);

    return 0;
}

void 
pci_msi_enable(pci_dev_t * dev)
{
    pci_set_msi(dev, 1);
}

void 
pci_msi_disable(pci_dev_t * dev)
{
    pci_set_msi(dev, 0);
}

static void 
pci_set_msix(pci_dev_t * dev, 
	     int         enable) 
{
    int pos     = dev->cfg.msix.msix_location;
    u16 control = 0;
    u16 new     = 0;

    control = pci_read(dev, pos + PCIR_MSIX_CTRL, 2);

    if (enable) {
        new = control |  PCIM_MSIXCTRL_MSIX_ENABLE;
    } else {
        new = control & ~PCIM_MSIXCTRL_MSIX_ENABLE;
    }

    if (new != control) {
        pci_write(dev, pos + PCIR_MSIX_CTRL, 2, new);
    } else {
        printk("Warning: MSI-X already %s!\n",
                enable ? "enabled" : "disabled");
    }
}

void 
pci_msix_enable(pci_dev_t * dev)
{
    pci_set_msix(dev, 1);
}

void 
pci_msix_disable(pci_dev_t * dev)
{
    pci_set_msix(dev, 0);
}

int 
pci_msix_setup(pci_dev_t         * dev, 
	       struct msix_entry * entries, 
	       u32                 num_entries)
{
    struct resource * msix_table_resource = NULL;
    pcicfg_hdr_t    * hdr                 = &dev->cfg;

    void __iomem    * msix_table_base     = NULL;

    int pos         = hdr->msix.msix_location;
    u16 msgctl      = 0;
    int max_entries = 0;

    int i = 0;
    int j = 0;

    /* check MSI-X capability */
    if (hdr->msix.valid == 0) {
        printk("MSI-X capability not found\n");
        return -1;
    }

    /* check number of entries supported */
    msgctl      = pci_read(dev, pos + PCIR_MSIX_CTRL, 2);
    max_entries = (msgctl & PCIM_MSIXCTRL_TABLE_SIZE) + 1;

    if (num_entries > max_entries) {
        printk("Number of vectors requested (%d) greater than supported (%d)\n",
                num_entries,
                max_entries);
        return -1;
    }

    /* validate vector and entries */
    for (i = 0; i < num_entries; i++) {
        if (entries[i].entry >= max_entries) {
            return -1;
        }
        for (j = i + 1; j < num_entries; j++) {
            if (entries[i].entry == entries[j].entry) {
                return -1;
            }
        }
    }

    /* TODO: check MSI is disabled */

    /* check MSI-X is disabled */
    if ((msgctl & PCIM_MSIXCTRL_MSIX_ENABLE) > 0) {
        printk("MSI-X already enabled\n");
        return -1;
    }

    /* map MSI-X table region */
    {
        u64 table_addr   = 0;
        u8  table_bir    = hdr->msix.msix_table_bar;
        u64 table_offset = hdr->msix.msix_table_offset;
        u64 table_size   = num_entries * PCI_MSIX_ENTRY_SIZE;

        /* table addr */
        u32 bar_low      = hdr->bar[table_bir];

        if (PCI_BAR_MEM(bar_low)) {
            u32 bar_type = bar_low & PCIM_BAR_MEM_TYPE;

            if (bar_type == PCIM_BAR_MEM_32) {
                table_addr = bar_low & PCIM_BAR_MEM_BASE;
            } else if (bar_type == PCIM_BAR_MEM_64) {
                //u32 bar_high = RREG(PCIR_BAR(table_bir+1), 4);
                u32 bar_high = hdr->bar[table_bir+1];

                table_addr = (bar_low & PCIM_BAR_MEM_BASE) | ((u64)bar_high << 32);
            } else {
                printk("Error: wrong MSI-X table bar type\n");
                return -1;
            }

            table_addr += table_offset;

        } else {
            printk("Error: MSI-X table should be a mem bar\n");
            return -1;
        }

        /* request iomem resource */
        {
            msix_table_resource = (struct resource *)kmem_alloc(sizeof(struct resource));

            if (msix_table_resource == NULL) {
                printk("Error allocating msix_table_resource\n");
                return -1;
            }

            msix_table_resource->name   =  "MSI-X Vector Table";
            msix_table_resource->start  =  table_addr;
            msix_table_resource->end    =  table_addr + table_size;
            msix_table_resource->flags  =  IORESOURCE_MEM;

            request_resource(&iomem_resource, msix_table_resource);
        }

        msix_table_base = ioremap(table_addr, table_size);

        printk("MSI-X table address: va %p, pa %p,  [size %p]\n"	\
	       "  bar[%u]: %x; bar[%u+1]: %x\n"				\
	       "  table offset: %u\n"					\
	       "  number of entries: %d\n",
	       (void *)msix_table_base, 
	       (void *)table_addr, 
	       (void *)table_size,
	       table_bir,
	       (unsigned int)hdr->bar[table_bir], 
	       table_bir,
	       (unsigned int)hdr->bar[table_bir + 1],
	       (unsigned int)hdr->msix.msix_table_offset, 
	       num_entries);
    }

    /* update hardware table entries */
    {
        struct msi_msg msg;

        void __iomem * base = NULL;

        /* use boot cpu as the destination */
        int dest = cpu_info[0].arch.apic_id;

        for (i = 0; i < num_entries; i++) {
            compose_msi_msg(&msg, dest, entries[i].vector);

            base = msix_table_base + (i * PCI_MSIX_ENTRY_SIZE);

            writel(msg.address_lo, base + PCI_MSIX_ENTRY_LOWER_ADDR_OFFSET);
            writel(msg.address_hi, base + PCI_MSIX_ENTRY_UPPER_ADDR_OFFSET);
            writel(msg.data,       base + PCI_MSIX_ENTRY_DATA_OFFSET);
        }
    }

    /* enable MSI-X with device masked */
    msgctl |= PCIM_MSIXCTRL_MSIX_ENABLE | PCIM_MSIXCTRL_FUNCTION_MASK;
    pci_write(dev, pos + PCIR_MSIX_CTRL, 2, msgctl);

    /* disable INTx and unmask device */
    pci_intx_disable(dev);

    msgctl &= ~PCIM_MSIXCTRL_FUNCTION_MASK;
    pci_write(dev, pos + PCIR_MSIX_CTRL, 2, msgctl);

    return 0;
}


/** Registers a PCI device driver with the LWK. */
int
pci_register_driver(pci_driver_t * driver)
{
	pci_dev_t          * dev = NULL;
	pci_dev_id_t const * id  = NULL;

	// Register the driver
	spin_lock(&pci_lock);
	{
	    list_add(&driver->next, &pci_drivers);
	}
	spin_unlock(&pci_lock);	

	// Look for devices that the driver can handle
	list_for_each_entry(dev, &pci_devices, next) {

		if ((driver = pci_find_driver(dev, &id)) != NULL) {
			// Found a match. Call the drivers probe() function.
			printk(KERN_DEBUG "pci_register_driver found match, calling probe.\n");
			dev->driver = driver;
			driver->probe(dev, id);
			return 0;
		}
	}
	
	return 0;
}


/** Finds a PCI driver suitable for the PCI device passed in. */
pci_driver_t *
pci_find_driver(const pci_dev_t     * dev, 
		const pci_dev_id_t ** idp)
{
	pci_dev_id_t const * id     = NULL;
	pci_driver_t       * driver = NULL;

        uint16_t vendor_id = dev->cfg.vendor_id;
        uint64_t device_id = dev->cfg.device_id;

	spin_lock(&pci_lock);
	{
		list_for_each_entry(driver, &pci_drivers, next) {
			
			for (id = driver->id_table ; id->vendor_id != 0 ; id++) {
				
				if ((vendor_id == id->vendor_id) && 
				    (device_id == id->device_id)) {
					*idp = id;
					spin_unlock(&pci_lock);
					return driver;
				}
			}
		}
	}
	spin_unlock(&pci_lock);	

	return NULL;
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
