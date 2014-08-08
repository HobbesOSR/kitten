/*
 *	Low-Level PCI Support for PC
 *
 *	(c) 1999--2000 Martin Mares <mj@ucw.cz>
 */

#include <linux/sched.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/dmi.h>


#include <asm/acpi.h>
#include <asm/segment.h>
#include <asm/io.h>
#include <asm/smp.h>

#include "pci.h"

unsigned int pci_probe = PCI_PROBE_BIOS | PCI_PROBE_CONF1 | PCI_PROBE_CONF2 |
				PCI_PROBE_MMCONF;

unsigned int pci_early_dump_regs;
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

/*
 * This interrupt-safe spinlock protects all accesses to PCI
 * configuration space.
 */
DEFINE_SPINLOCK(pci_config_lock);

static int __devinit can_skip_ioresource_align(const struct dmi_system_id *d)
{
	pci_probe |= PCI_CAN_SKIP_ISA_ALIGN;
	printk(KERN_INFO "PCI: %s detected, can skip ISA alignment\n", d->ident);
	return 0;
}

static struct dmi_system_id can_skip_pciprobe_dmi_table[] __devinitdata = {
/*
 * Systems where PCI IO resource ISA alignment can be skipped
 * when the ISA enable bit in the bridge control is not set
 */
	{
		.callback = can_skip_ioresource_align,
		.ident = "IBM System x3800",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "IBM"),
			DMI_MATCH(DMI_PRODUCT_NAME, "x3800"),
		},
	},
	{
		.callback = can_skip_ioresource_align,
		.ident = "IBM System x3850",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "IBM"),
			DMI_MATCH(DMI_PRODUCT_NAME, "x3850"),
		},
	},
	{
		.callback = can_skip_ioresource_align,
		.ident = "IBM System x3950",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "IBM"),
			DMI_MATCH(DMI_PRODUCT_NAME, "x3950"),
		},
	},
	{}
};

void __init dmi_check_skip_isa_align(void)
{
	dmi_check_system(can_skip_pciprobe_dmi_table);
}

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

/*
 * Only use DMI information to set this if nothing was passed
 * on the kernel command line (which was parsed earlier).
 */

static int __devinit set_bf_sort(const struct dmi_system_id *d)
{
	if (pci_bf_sort == pci_bf_sort_default) {
		pci_bf_sort = pci_dmi_bf;
		printk(KERN_INFO "PCI: %s detected, enabling pci=bfsort.\n", d->ident);
	}
	return 0;
}

/*
 * Enable renumbering of PCI bus# ranges to reach all PCI busses (Cardbus)
 */
#ifdef __i386__
static int __devinit assign_all_busses(const struct dmi_system_id *d)
{
	pci_probe |= PCI_ASSIGN_ALL_BUSSES;
	printk(KERN_INFO "%s detected: enabling PCI bus# renumbering"
			" (pci=assign-busses)\n", d->ident);
	return 0;
}
#endif

static struct dmi_system_id __devinitdata pciprobe_dmi_table[] = {
#ifdef __i386__
/*
 * Laptops which need pci=assign-busses to see Cardbus cards
 */
	{
		.callback = assign_all_busses,
		.ident = "Samsung X20 Laptop",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Samsung Electronics"),
			DMI_MATCH(DMI_PRODUCT_NAME, "SX20S"),
		},
	},
#endif		/* __i386__ */
	{
		.callback = set_bf_sort,
		.ident = "Dell PowerEdge 1950",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell"),
			DMI_MATCH(DMI_PRODUCT_NAME, "PowerEdge 1950"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "Dell PowerEdge 1955",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell"),
			DMI_MATCH(DMI_PRODUCT_NAME, "PowerEdge 1955"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "Dell PowerEdge 2900",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell"),
			DMI_MATCH(DMI_PRODUCT_NAME, "PowerEdge 2900"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "Dell PowerEdge 2950",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell"),
			DMI_MATCH(DMI_PRODUCT_NAME, "PowerEdge 2950"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "Dell PowerEdge R900",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Dell"),
			DMI_MATCH(DMI_PRODUCT_NAME, "PowerEdge R900"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant BL20p G3",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant BL20p G3"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant BL20p G4",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant BL20p G4"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant BL30p G1",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant BL30p G1"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant BL25p G1",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant BL25p G1"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant BL35p G1",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant BL35p G1"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant BL45p G1",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant BL45p G1"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant BL45p G2",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant BL45p G2"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant BL460c G1",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant BL460c G1"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant BL465c G1",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant BL465c G1"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant BL480c G1",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant BL480c G1"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant BL685c G1",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant BL685c G1"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant DL360",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant DL360"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant DL380",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant DL380"),
		},
	},
#ifdef __i386__
	{
		.callback = assign_all_busses,
		.ident = "Compaq EVO N800c",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "Compaq"),
			DMI_MATCH(DMI_PRODUCT_NAME, "EVO N800c"),
		},
	},
#endif
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant DL385 G2",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant DL385 G2"),
		},
	},
	{
		.callback = set_bf_sort,
		.ident = "HP ProLiant DL585 G2",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "HP"),
			DMI_MATCH(DMI_PRODUCT_NAME, "ProLiant DL585 G2"),
		},
	},
	{}
};

void __init dmi_check_pciprobe(void)
{
	dmi_check_system(pciprobe_dmi_table);
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

extern u8 pci_cache_line_size;

int __init pcibios_init(void)
{
	struct arch_cpuinfo *c = &boot_cpu_data.arch;


	/*
	 * Assume PCI cacheline size of 32 bytes for all x86s except K7/K8
	 * and P4. It's also good for 386/486s (which actually have 16)
	 * as quite a few PCI devices do not support smaller values.
	 */
	pci_cache_line_size = 32 >> 2;
	if (c->x86_family >= 6 && c->x86_vendor == X86_VENDOR_AMD)
		pci_cache_line_size = 64 >> 2;	/* K7 & K8 */
	else if (c->x86_family > 6 && c->x86_vendor == X86_VENDOR_INTEL)
		pci_cache_line_size = 128 >> 2;	/* P4 */

	pcibios_resource_survey();

	if (pci_bf_sort >= pci_force_bf)
		pci_sort_breadthfirst();
	return 0;
}

char * __devinit  pcibios_setup(char *str)
{
	if (!strcmp(str, "off")) {
		pci_probe = 0;
		return NULL;
	} else if (!strcmp(str, "bfsort")) {
		pci_bf_sort = pci_force_bf;
		return NULL;
	} else if (!strcmp(str, "nobfsort")) {
		pci_bf_sort = pci_force_nobf;
		return NULL;
	}
#ifdef CONFIG_PCI_BIOS
	else if (!strcmp(str, "bios")) {
		pci_probe = PCI_PROBE_BIOS;
		return NULL;
	} else if (!strcmp(str, "nobios")) {
		pci_probe &= ~PCI_PROBE_BIOS;
		return NULL;
	} else if (!strcmp(str, "biosirq")) {
		pci_probe |= PCI_BIOS_IRQ_SCAN;
		return NULL;
	} else if (!strncmp(str, "pirqaddr=", 9)) {
		pirq_table_addr = simple_strtoul(str+9, NULL, 0);
		return NULL;
	}
#endif
#ifdef CONFIG_PCI_DIRECT
	else if (!strcmp(str, "conf1")) {
		pci_probe = PCI_PROBE_CONF1 | PCI_NO_CHECKS;
		return NULL;
	}
	else if (!strcmp(str, "conf2")) {
		pci_probe = PCI_PROBE_CONF2 | PCI_NO_CHECKS;
		return NULL;
	}
#endif
#ifdef CONFIG_PCI_MMCONFIG
	else if (!strcmp(str, "nommconf")) {
		pci_probe &= ~PCI_PROBE_MMCONF;
		return NULL;
	}
	else if (!strcmp(str, "check_enable_amd_mmconf")) {
		pci_probe |= PCI_CHECK_ENABLE_AMD_MMCONF;
		return NULL;
	}
#endif
	else if (!strcmp(str, "noacpi")) {
		acpi_noirq_set();
		return NULL;
	}
	else if (!strcmp(str, "noearly")) {
		pci_probe |= PCI_PROBE_NOEARLY;
		return NULL;
	}
#ifndef CONFIG_X86_VISWS
	else if (!strcmp(str, "usepirqmask")) {
		pci_probe |= PCI_USE_PIRQ_MASK;
		return NULL;
	} else if (!strncmp(str, "irqmask=", 8)) {
		pcibios_irq_mask = simple_strtol(str+8, NULL, 0);
		return NULL;
	} else if (!strncmp(str, "lastbus=", 8)) {
		pcibios_last_bus = simple_strtol(str+8, NULL, 0);
		return NULL;
	}
#endif
	else if (!strcmp(str, "rom")) {
		pci_probe |= PCI_ASSIGN_ROMS;
		return NULL;
	} else if (!strcmp(str, "norom")) {
		pci_probe |= PCI_NOASSIGN_ROMS;
		return NULL;
	} else if (!strcmp(str, "assign-busses")) {
		pci_probe |= PCI_ASSIGN_ALL_BUSSES;
		return NULL;
	} else if (!strcmp(str, "use_crs")) {
		pci_probe |= PCI_USE__CRS;
		return NULL;
	} else if (!strcmp(str, "earlydump")) {
		pci_early_dump_regs = 1;
		return NULL;
	} else if (!strcmp(str, "routeirq")) {
		pci_routeirq = 1;
		return NULL;
	} else if (!strcmp(str, "skip_isa_align")) {
		pci_probe |= PCI_CAN_SKIP_ISA_ALIGN;
		return NULL;
	}
	return str;
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
