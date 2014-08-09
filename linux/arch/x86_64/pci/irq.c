/*
 *	Low-Level PCI Support for PC -- Routing of Interrupts
 *
 *	(c) 1999--2000 Martin Mares <mj@ucw.cz>
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/smp.h>
#include <asm/io_apic.h>
#include <linux/irq.h>
#include <arch/acpi.h> 

#include "pci.h"

#define PIRQ_SIGNATURE	(('$' << 0) + ('P' << 8) + ('I' << 16) + ('R' << 24))
#define PIRQ_VERSION 0x0100


static struct irq_routing_table *pirq_table;

static int pirq_enable_irq(struct pci_dev *dev);

/*
 * Never use: 0, 1, 2 (timer, keyboard, and cascade)
 * Avoid using: 13, 14 and 15 (FP error and IDE).
 * Penalize: 3, 4, 6, 7, 12 (known ISA uses: serial, floppy, parallel and mouse)
 */
unsigned int pcibios_irq_mask = 0xfff8;

static int pirq_penalty[16] = {
	1000000, 1000000, 1000000, 1000, 1000, 0, 1000, 1000,
	0, 0, 0, 0, 1000, 100000, 100000, 100000
};

struct irq_router {
	char *name;
	u16 vendor, device;
	int (*get)(struct pci_dev *router, struct pci_dev *dev, int pirq);
	int (*set)(struct pci_dev *router, struct pci_dev *dev, int pirq,
		int new);
};

struct irq_router_handler {
	u16 vendor;
	int (*probe)(struct irq_router *r, struct pci_dev *router, u16 device);
};

int (*pcibios_enable_irq)(struct pci_dev *dev) = NULL;
void (*pcibios_disable_irq)(struct pci_dev *dev) = NULL;

/*
 *  Check passed address for the PCI IRQ Routing Table signature
 *  and perform checksum verification.
 */

static inline struct irq_routing_table *pirq_check_routing_table(u8 *addr)
{
	struct irq_routing_table *rt;
	int i;
	u8 sum;

	rt = (struct irq_routing_table *) addr;
	if (rt->signature != PIRQ_SIGNATURE ||
	    rt->version != PIRQ_VERSION ||
	    rt->size % 16 ||
	    rt->size < sizeof(struct irq_routing_table))
		return NULL;
	sum = 0;
	for (i = 0; i < rt->size; i++)
		sum += addr[i];
	if (!sum) {
		DBG(KERN_DEBUG "PCI: Interrupt Routing Table found at 0x%p\n",
			rt);
		return rt;
	}
	return NULL;
}



/*
 *  Search 0xf0000 -- 0xfffff for the PCI IRQ Routing Table.
 */

static struct irq_routing_table * __init pirq_find_routing_table(void)
{
	u8 *addr;
	struct irq_routing_table *rt;

	if (pirq_table_addr) {
		rt = pirq_check_routing_table((u8 *) __va(pirq_table_addr));
		if (rt)
			return rt;
		printk(KERN_WARNING "PCI: PIRQ table NOT found at pirqaddr\n");
	}
	for (addr = (u8 *) __va(0xf0000); addr < (u8 *) __va(0x100000); addr += 16) {
		rt = pirq_check_routing_table(addr);
		if (rt)
			return rt;
	}
	return NULL;
}

/*
 *  If we have a IRQ routing table, use it to search for peer host
 *  bridges.  It's a gross hack, but since there are no other known
 *  ways how to get a list of buses, we have to go this way.
 */

static void __init pirq_peer_trick(void)
{
	struct irq_routing_table *rt = pirq_table;
	u8 busmap[256];
	int i;
	struct irq_info *e;

	memset(busmap, 0, sizeof(busmap));
	for (i = 0; i < (rt->size - sizeof(struct irq_routing_table)) / sizeof(struct irq_info); i++) {
		e = &rt->slots[i];
#ifdef DEBUG
		{
			int j;
			DBG(KERN_DEBUG "%02x:%02x slot=%02x", e->bus, e->devfn/8, e->slot);
			for (j = 0; j < 4; j++)
				DBG(" %d:%02x/%04x", j, e->irq[j].link, e->irq[j].bitmap);
			DBG("\n");
		}
#endif
		busmap[e->bus] = 1;
	}
	for (i = 1; i < 256; i++) {
		int node;
		if (!busmap[i] || pci_find_bus(0, i))
			continue;
		node = get_mp_bus_to_node(i);
		if (pci_scan_bus_on_node(i, &pci_root_ops, node))
			printk(KERN_INFO "PCI: Discovered primary peer "
			       "bus %02x [IRQ]\n", i);
	}
	pcibios_last_bus = -1;
}

/*
 *  Code for querying and setting of IRQ routes on various interrupt routers.
 */

void eisa_set_level_irq(unsigned int irq)
{
	unsigned char mask = 1 << (irq & 7);
	unsigned int port = 0x4d0 + (irq >> 3);
	unsigned char val;
	static u16 eisa_irq_mask;

	if (irq >= 16 || (1 << irq) & eisa_irq_mask)
		return;

	eisa_irq_mask |= (1 << irq);
	printk(KERN_DEBUG "PCI: setting IRQ %u as level-triggered\n", irq);
	val = inb(port);
	if (!(val & mask)) {
		DBG(KERN_DEBUG " -> edge");
		outb(val | mask, port);
	}
}



/*
 * The Intel PIIX4 pirq rules are fairly simple: "pirq" is
 * just a pointer to the config space.
 */
static int pirq_piix_get(struct pci_dev *router, struct pci_dev *dev, int pirq)
{
	u8 x;

	pci_read_config_byte(router, pirq, &x);
	return (x < 16) ? x : 0;
}

static int pirq_piix_set(struct pci_dev *router, struct pci_dev *dev, int pirq, int irq)
{
	pci_write_config_byte(router, pirq, irq);
	return 1;
}





static __init int intel_router_probe(struct irq_router *r, struct pci_dev *router, u16 device)
{
	static struct pci_device_id __initdata pirq_440gx[] = {
		{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82443GX_0) },
		{ PCI_DEVICE(PCI_VENDOR_ID_INTEL, PCI_DEVICE_ID_INTEL_82443GX_2) },
		{ },
	};

	/* 440GX has a proprietary PIRQ router -- don't use it */
	if (pci_dev_present(pirq_440gx))
		return 0;

	switch (device) {
	case PCI_DEVICE_ID_INTEL_82371FB_0:
	case PCI_DEVICE_ID_INTEL_82371SB_0:
	case PCI_DEVICE_ID_INTEL_82371AB_0:
	case PCI_DEVICE_ID_INTEL_82371MX:
	case PCI_DEVICE_ID_INTEL_82443MX_0:
	case PCI_DEVICE_ID_INTEL_82801AA_0:
	case PCI_DEVICE_ID_INTEL_82801AB_0:
	case PCI_DEVICE_ID_INTEL_82801BA_0:
	case PCI_DEVICE_ID_INTEL_82801BA_10:
	case PCI_DEVICE_ID_INTEL_82801CA_0:
	case PCI_DEVICE_ID_INTEL_82801CA_12:
	case PCI_DEVICE_ID_INTEL_82801DB_0:
	case PCI_DEVICE_ID_INTEL_82801E_0:
	case PCI_DEVICE_ID_INTEL_82801EB_0:
	case PCI_DEVICE_ID_INTEL_ESB_1:
	case PCI_DEVICE_ID_INTEL_ICH6_0:
	case PCI_DEVICE_ID_INTEL_ICH6_1:
	case PCI_DEVICE_ID_INTEL_ICH7_0:
	case PCI_DEVICE_ID_INTEL_ICH7_1:
	case PCI_DEVICE_ID_INTEL_ICH7_30:
	case PCI_DEVICE_ID_INTEL_ICH7_31:
	case PCI_DEVICE_ID_INTEL_TGP_LPC:
	case PCI_DEVICE_ID_INTEL_ESB2_0:
	case PCI_DEVICE_ID_INTEL_ICH8_0:
	case PCI_DEVICE_ID_INTEL_ICH8_1:
	case PCI_DEVICE_ID_INTEL_ICH8_2:
	case PCI_DEVICE_ID_INTEL_ICH8_3:
	case PCI_DEVICE_ID_INTEL_ICH8_4:
	case PCI_DEVICE_ID_INTEL_ICH9_0:
	case PCI_DEVICE_ID_INTEL_ICH9_1:
	case PCI_DEVICE_ID_INTEL_ICH9_2:
	case PCI_DEVICE_ID_INTEL_ICH9_3:
	case PCI_DEVICE_ID_INTEL_ICH9_4:
	case PCI_DEVICE_ID_INTEL_ICH9_5:
	case PCI_DEVICE_ID_INTEL_TOLAPAI_0:
	case PCI_DEVICE_ID_INTEL_ICH10_0:
	case PCI_DEVICE_ID_INTEL_ICH10_1:
	case PCI_DEVICE_ID_INTEL_ICH10_2:
	case PCI_DEVICE_ID_INTEL_ICH10_3:
	case PCI_DEVICE_ID_INTEL_PCH_0:
	case PCI_DEVICE_ID_INTEL_PCH_1:
		r->name = "PIIX/ICH";
		r->get = pirq_piix_get;
		r->set = pirq_piix_set;
		return 1;
	}
	return 0;
}




static __initdata struct irq_router_handler pirq_routers[] = {
	{ PCI_VENDOR_ID_INTEL, intel_router_probe },
	/* Someone with docs needs to add the ATI Radeon IGP */
	{ 0, NULL }
};
static struct irq_router pirq_router;
static struct pci_dev *pirq_router_dev;


/*
 *	FIXME: should we have an option to say "generic for
 *	chipset" ?
 */

static void __init pirq_find_router(struct irq_router *r)
{
	struct irq_routing_table *rt = pirq_table;
	struct irq_router_handler *h;

#ifdef CONFIG_PCI_BIOS
	if (!rt->signature) {
		printk(KERN_INFO "PCI: Using BIOS for IRQ routing\n");
		r->set = pirq_bios_set;
		r->name = "BIOS";
		return;
	}
#endif

	/* Default unless a driver reloads it */
	r->name = "default";
	r->get = NULL;
	r->set = NULL;

	DBG(KERN_DEBUG "PCI: Attempting to find IRQ router for %04x:%04x\n",
	    rt->rtr_vendor, rt->rtr_device);

	pirq_router_dev = pci_get_bus_and_slot(rt->rtr_bus, rt->rtr_devfn);
	if (!pirq_router_dev) {
		DBG(KERN_DEBUG "PCI: Interrupt router not found at "
			"%02x:%02x\n", rt->rtr_bus, rt->rtr_devfn);
		return;
	}

	for (h = pirq_routers; h->vendor; h++) {
		/* First look for a router match */
		if (rt->rtr_vendor == h->vendor &&
			h->probe(r, pirq_router_dev, rt->rtr_device))
			break;
		/* Fall back to a device match */
		if (pirq_router_dev->vendor == h->vendor &&
			h->probe(r, pirq_router_dev, pirq_router_dev->device))
			break;
	}
	dev_info(&pirq_router_dev->dev, "%s IRQ router [%04x/%04x]\n",
		 pirq_router.name,
		 pirq_router_dev->vendor, pirq_router_dev->device);

	/* The device remains referenced for the kernel lifetime */
}

static struct irq_info *pirq_get_info(struct pci_dev *dev)
{
	struct irq_routing_table *rt = pirq_table;
	int entries = (rt->size - sizeof(struct irq_routing_table)) /
		sizeof(struct irq_info);
	struct irq_info *info;

	for (info = rt->slots; entries--; info++)
		if (info->bus == dev->bus->number &&
			PCI_SLOT(info->devfn) == PCI_SLOT(dev->devfn))
			return info;
	return NULL;
}

static int pcibios_lookup_irq(struct pci_dev *dev, int assign)
{
	u8 pin;
	struct irq_info *info;
	int i, pirq, newirq;
	int irq = 0;
	u32 mask;
	struct irq_router *r = &pirq_router;
	struct pci_dev *dev2 = NULL;
	char *msg = NULL;

	/* Find IRQ pin */
	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
	if (!pin) {
		dev_dbg(&dev->dev, "no interrupt pin\n");
		return 0;
	}
	pin = pin - 1;

	/* Find IRQ routing entry */

	if (!pirq_table)
		return 0;

	info = pirq_get_info(dev);
	if (!info) {
		dev_dbg(&dev->dev, "PCI INT %c not found in routing table\n",
			'A' + pin);
		return 0;
	}
	pirq = info->irq[pin].link;
	mask = info->irq[pin].bitmap;
	if (!pirq) {
		dev_dbg(&dev->dev, "PCI INT %c not routed\n", 'A' + pin);
		return 0;
	}
	dev_dbg(&dev->dev, "PCI INT %c -> PIRQ %02x, mask %04x, excl %04x",
		'A' + pin, pirq, mask, pirq_table->exclusive_irqs);
	mask &= pcibios_irq_mask;


	/*
	 * Find the best IRQ to assign: use the one
	 * reported by the device if possible.
	 */
	newirq = dev->irq;
	if (newirq && !((1 << newirq) & mask)) {
		if (pci_probe & PCI_USE_PIRQ_MASK)
			newirq = 0;
		else
			dev_warn(&dev->dev, "IRQ %d doesn't match PIRQ mask "
				 "%#x; try pci=usepirqmask\n", newirq, mask);
	}
	if (!newirq && assign) {
		for (i = 0; i < 16; i++) {
			if (!(mask & (1 << i)))
				continue;
			if (pirq_penalty[i] < pirq_penalty[newirq] &&
				can_request_irq(i, IRQF_SHARED))
				newirq = i;
		}
	}
	dev_dbg(&dev->dev, "PCI INT %c -> newirq %d", 'A' + pin, newirq);

	/* Check if it is hardcoded */
	if ((pirq & 0xf0) == 0xf0) {
		irq = pirq & 0xf;
		msg = "hardcoded";
	} else if (r->get && (irq = r->get(pirq_router_dev, dev, pirq)) && \
	((!(pci_probe & PCI_USE_PIRQ_MASK)) || ((1 << irq) & mask))) {
		msg = "found";
		eisa_set_level_irq(irq);
	} else if (newirq && r->set &&
		(dev->class >> 8) != PCI_CLASS_DISPLAY_VGA) {
		if (r->set(pirq_router_dev, dev, pirq, newirq)) {
			eisa_set_level_irq(newirq);
			msg = "assigned";
			irq = newirq;
		}
	}

	if (!irq) {
		if (newirq && mask == (1 << newirq)) {
			msg = "guessed";
			irq = newirq;
		} else {
			dev_dbg(&dev->dev, "can't route interrupt\n");
			return 0;
		}
	}
	dev_info(&dev->dev, "%s PCI INT %c -> IRQ %d\n", msg, 'A' + pin, irq);

	/* Update IRQ for all devices with the same pirq value */
	while ((dev2 = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev2)) != NULL) {
		pci_read_config_byte(dev2, PCI_INTERRUPT_PIN, &pin);
		if (!pin)
			continue;
		pin--;
		info = pirq_get_info(dev2);
		if (!info)
			continue;
		if (info->irq[pin].link == pirq) {
			/*
			 * We refuse to override the dev->irq
			 * information. Give a warning!
			 */
			if (dev2->irq && dev2->irq != irq && \
			(!(pci_probe & PCI_USE_PIRQ_MASK) || \
			((1 << dev2->irq) & mask))) {
				continue;
			}
			dev2->irq = irq;
			pirq_penalty[irq]++;
			if (dev != dev2)
				dev_info(&dev->dev, "sharing IRQ %d with %s\n",
					 irq, pci_name(dev2));
		}
	}
	return 1;
}

static void __init pcibios_fixup_irqs(void)
{
	struct pci_dev *dev = NULL;
	u8 pin;

	DBG(KERN_DEBUG "PCI: IRQ fixup\n");
	while ((dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev)) != NULL) {
		/*
		 * If the BIOS has set an out of range IRQ number, just
		 * ignore it.  Also keep track of which IRQ's are
		 * already in use.
		 */
		if (dev->irq >= 16) {
			dev_dbg(&dev->dev, "ignoring bogus IRQ %d\n", dev->irq);
			dev->irq = 0;
		}
		/*
		 * If the IRQ is already assigned to a PCI device,
		 * ignore its ISA use penalty
		 */
		if (pirq_penalty[dev->irq] >= 100 &&
				pirq_penalty[dev->irq] < 100000)
			pirq_penalty[dev->irq] = 0;
		pirq_penalty[dev->irq]++;
	}

	dev = NULL;
	while ((dev = pci_get_device(PCI_ANY_ID, PCI_ANY_ID, dev)) != NULL) {
		pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
#ifdef CONFIG_X86_IO_APIC
		/*
		 * Recalculate IRQ numbers if we use the I/O APIC.
		 */
		if (io_apic_assign_pci_irqs) {
			int irq;

			if (pin) {
				/*
				 * interrupt pins are numbered starting
				 * from 1
				 */
				pin--;
				irq = IO_APIC_get_PCI_irq_vector(dev->bus->number,
					PCI_SLOT(dev->devfn), pin);
	/*
	 * Busses behind bridges are typically not listed in the MP-table.
	 * In this case we have to look up the IRQ based on the parent bus,
	 * parent slot, and pin number. The SMP code detects such bridged
	 * busses itself so we should get into this branch reliably.
	 */
				if (irq < 0 && dev->bus->parent) { /* go back to the bridge */
					struct pci_dev *bridge = dev->bus->self;

					pin = (pin + PCI_SLOT(dev->devfn)) % 4;
					irq = IO_APIC_get_PCI_irq_vector(bridge->bus->number,
							PCI_SLOT(bridge->devfn), pin);
					if (irq >= 0)
						dev_warn(&dev->dev, "using bridge %s INT %c to get IRQ %d\n",
							 pci_name(bridge),
							 'A' + pin, irq);
				}
				if (irq >= 0) {
					dev_info(&dev->dev, "PCI->APIC IRQ transform: INT %c -> IRQ %d\n", 'A' + pin, irq);
					dev->irq = irq;
				}
			}
		}
#endif
		/*
		 * Still no IRQ? Try to lookup one...
		 */
		if (pin && !dev->irq)
			pcibios_lookup_irq(dev, 0);
	}
}



int __init pcibios_irq_init(void)
{
	DBG(KERN_DEBUG "PCI: IRQ init\n");

	if (pcibios_enable_irq)
		return 0;

	pirq_table = pirq_find_routing_table();

#ifdef CONFIG_PCI_BIOS
	if (!pirq_table && (pci_probe & PCI_BIOS_IRQ_SCAN))
		pirq_table = pcibios_get_irq_routing_table();
#endif
	if (pirq_table) {
		pirq_peer_trick();
		pirq_find_router(&pirq_router);
		if (pirq_table->exclusive_irqs) {
			int i;
			for (i = 0; i < 16; i++)
				if (!(pirq_table->exclusive_irqs & (1 << i)))
					pirq_penalty[i] += 100;
		}
		/*
		 * If we're using the I/O APIC, avoid using the PCI IRQ
		 * routing table
		 */
		if (io_apic_assign_pci_irqs)
			pirq_table = NULL;
	}

	pcibios_enable_irq = pirq_enable_irq;

	pcibios_fixup_irqs();
	return 0;
}

static void pirq_penalize_isa_irq(int irq, int active)
{
	/*
	 *  If any ISAPnP device reports an IRQ in its list of possible
	 *  IRQ's, we try to avoid assigning it to PCI devices.
	 */
	if (irq < 16) {
		if (active)
			pirq_penalty[irq] += 1000;
		else
			pirq_penalty[irq] += 100;
	}
}

void pcibios_penalize_isa_irq(int irq, int active)
{
        pirq_penalize_isa_irq(irq, active);
}

static int pirq_enable_irq(struct pci_dev *dev)
{
	u8 pin;
	struct pci_dev *temp_dev;

	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
	if (pin && !pcibios_lookup_irq(dev, 1) && !dev->irq) {
		char *msg = "";

		pin--; /* interrupt pins are numbered starting from 1 */

		if (io_apic_assign_pci_irqs) {
			int irq;

			irq = IO_APIC_get_PCI_irq_vector(dev->bus->number, PCI_SLOT(dev->devfn), pin);
			/*
			 * Busses behind bridges are typically not listed in the MP-table.
			 * In this case we have to look up the IRQ based on the parent bus,
			 * parent slot, and pin number. The SMP code detects such bridged
			 * busses itself so we should get into this branch reliably.
			 */
			temp_dev = dev;
			while (irq < 0 && dev->bus->parent) { /* go back to the bridge */
				struct pci_dev *bridge = dev->bus->self;

				pin = (pin + PCI_SLOT(dev->devfn)) % 4;
				irq = IO_APIC_get_PCI_irq_vector(bridge->bus->number,
						PCI_SLOT(bridge->devfn), pin);
				if (irq >= 0)
					dev_warn(&dev->dev, "using bridge %s "
						 "INT %c to get IRQ %d\n",
						 pci_name(bridge), 'A' + pin,
						 irq);
				dev = bridge;
			}
			dev = temp_dev;
			if (irq >= 0) {
				dev_info(&dev->dev, "PCI->APIC IRQ transform: "
					 "INT %c -> IRQ %d\n", 'A' + pin, irq);
				dev->irq = irq;
				return 0;
			} else
				msg = "; probably buggy MP table";
		} else if (pci_probe & PCI_BIOS_IRQ_SCAN)
			msg = "";
		else
			msg = "; please try using pci=biosirq";

		/*
		 * With IDE legacy devices the IRQ lookup failure is not
		 * a problem..
		 */
		if (dev->class >> 8 == PCI_CLASS_STORAGE_IDE &&
				!(dev->class & 0x5))
			return 0;

		dev_warn(&dev->dev, "can't find IRQ for PCI INT %c%s\n",
			 'A' + pin, msg);
	}
	return 0;
}
