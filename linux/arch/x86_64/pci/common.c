/*
 *	Low-Level PCI Support for PC
 *
 *	(c) 1999--2000 Martin Mares <mj@ucw.cz>
 */

#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/init.h>


#include <asm/acpi.h>
#include <asm/segment.h>
#include <asm/io.h>
#include <asm/smp.h>

#include "pci.h"

unsigned int pci_probe = 0;

static int pci_bf_sort;
int pci_routeirq;
int pcibios_last_bus = -1;
unsigned long pirq_table_addr;
struct pci_bus *pci_root_bus;


static int pci_read(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 *value)
{
	return raw_pci_read(pci_domain_nr(bus), bus->number,
			    devfn, where, size, value);
}

static int pci_write(struct pci_bus *bus, unsigned int devfn, int where, int size, u32 value)
{
	return raw_pci_write(pci_domain_nr(bus), bus->number,
			     devfn, where, size, value);
}

struct pci_ops pci_root_ops = {
	.read = pci_read,
	.write = pci_write,
};

/*
 * legacy, numa, and acpi all want to call pcibios_scan_root
 * from their initcalls. This flag prevents that.
 */
int pcibios_scanned;




static void __devinit pcibios_fixup_device_resources(struct pci_dev *dev)
{
	struct resource *rom_r = &dev->resource[PCI_ROM_RESOURCE];

	if (pci_probe & PCI_NOASSIGN_ROMS) {
		if (rom_r->parent)
			return;
		if (rom_r->start) {
			/* we deal with BIOS assigned ROM later */
			return;
		}
		rom_r->start = rom_r->end = rom_r->flags = 0;
	}
}

/*
 *  Called after each bus is probed, but before its children
 *  are examined.
 */

void __devinit  pcibios_fixup_bus(struct pci_bus *b)
{
	struct pci_dev *dev;

	pci_read_bridge_bases(b);
	list_for_each_entry(dev, &b->devices, bus_list)
		pcibios_fixup_device_resources(dev);
}




struct pci_bus * __devinit pcibios_scan_root(int busnum)
{
	struct pci_bus *bus = NULL;
	struct pci_sysdata *sd;

	while ((bus = pci_find_next_bus(bus)) != NULL) {
		if (bus->number == busnum) {
			/* Already scanned */
			return bus;
		}
	}

	/* Allocate per-root-bus (not per bus) arch-specific data.
	 * TODO: leak; this memory is never freed.
	 * It's arguable whether it's worth the trouble to care.
	 */
	sd = kzalloc(sizeof(*sd), GFP_KERNEL);
	if (!sd) {
		printk(KERN_ERR "PCI: OOM, not probing PCI bus %02x\n", busnum);
		return NULL;
	}

	sd->node = get_mp_bus_to_node(busnum);

	printk(KERN_DEBUG "PCI: Probing PCI hardware (bus %02x)\n", busnum);
	bus = pci_scan_bus_parented(NULL, busnum, &pci_root_ops, sd);
	if (!bus)
		kfree(sd);

	return bus;
}


int __init pcibios_init(void)
{
	struct arch_cpuinfo *c = &boot_cpu_data.arch;

	pcibios_resource_survey();

	if (pci_bf_sort >= pci_force_bf)
		pci_sort_breadthfirst();
	return 0;
}


unsigned int pcibios_assign_all_busses(void)
{
	return (pci_probe & PCI_ASSIGN_ALL_BUSSES) ? 1 : 0;
}

int pcibios_enable_device(struct pci_dev *dev, int mask)
{
	int err;

	if ((err = pci_enable_resources(dev, mask)) < 0)
		return err;

	if (!dev->msi_enabled)
		return pcibios_enable_irq(dev);
	return 0;
}

void pcibios_disable_device (struct pci_dev *dev)
{
	if (!dev->msi_enabled && pcibios_disable_irq)
		pcibios_disable_irq(dev);
}

struct pci_bus * __devinit pci_scan_bus_on_node(int busno, struct pci_ops *ops, int node)
{
	struct pci_bus *bus = NULL;
	struct pci_sysdata *sd;

	/*
	 * Allocate per-root-bus (not per bus) arch-specific data.
	 * TODO: leak; this memory is never freed.
	 * It's arguable whether it's worth the trouble to care.
	 */
	sd = kzalloc(sizeof(*sd), GFP_KERNEL);
	if (!sd) {
		printk(KERN_ERR "PCI: OOM, skipping PCI bus %02x\n", busno);
		return NULL;
	}
	sd->node = node;
	bus = pci_scan_bus(busno, ops, sd);
	if (!bus)
		kfree(sd);

	return bus;
}

struct pci_bus * __devinit pci_scan_bus_with_sysdata(int busno)
{
	return pci_scan_bus_on_node(busno, &pci_root_ops, -1);
}
