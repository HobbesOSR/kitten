#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/resource.h>
#include <lwk/bootmem.h>
#include <lwk/spinlock.h>
#include <lwk/cpuinfo.h>
#include <arch/io.h>
#include <arch/pgtable.h>
#include <arch/fixmap.h>
#include <arch/apicdef.h>
#include <arch/io_apic.h>
#include <arch/idt_vectors.h>

/**
 * Lock that protects access to the IO APICs in the system.
 * There is only one lock for all IO APICs.
 */
static DEFINE_SPINLOCK(ioapic_lock);

/**
 * Number of IO APICs in the system.
 */
unsigned int ioapic_num;

/**
 * Information about each IO APIC in the system.
 * The array is indexed by ioapic_index.
 */
struct ioapic_info ioapic_info[MAX_IO_APICS];

/**
 * Resource entries for the IO APIC memory mapping.
 */
static struct resource *ioapic_resources;

/**
 * Structure used to map IO APIC registers.
 */
struct ioapic {
	uint32_t index;
	uint32_t unused[3];
	uint32_t data;
};

/**
 * Union used to map an IO APIC routing entry register.
 */
union ioapic_entry_union {
	struct { uint32_t low_word, high_word; };
	struct IO_APIC_route_entry entry;
};

/**
 * Returns the base kernel virtual address of the specified IO APIC's
 * kernel mapping.
 */
static struct ioapic *
ioapic_base_addr(int ioapic_index)
{
	return (void *) __fix_to_virt(FIX_IO_APIC_BASE_0 + ioapic_index)
		 + (ioapic_info[ioapic_index].phys_addr & ~PAGE_MASK);
}

/**
 * Reads a value from an IO APIC register.
 */
static uint32_t
ioapic_read(unsigned int ioapic_index, uint32_t reg)
{
	struct ioapic *ioapic = ioapic_base_addr(ioapic_index);
	writel(reg, &ioapic->index);
	return readl(&ioapic->data);
}

/**
 * Writes a value to an IO APIC register.
 */
static void
ioapic_write(unsigned int ioapic_index, uint32_t reg, uint32_t value)
{
	struct ioapic *ioapic = ioapic_base_addr(ioapic_index);
	writel(reg, &ioapic->index);
	writel(value, &ioapic->data);
}

/**
 * Reads an IO APIC pin routing entry.
 */
static struct IO_APIC_route_entry
ioapic_read_pin(
	unsigned int ioapic_index,
	unsigned int pin
)
{
	union ioapic_entry_union eu;
	unsigned long flags;

	spin_lock_irqsave(&ioapic_lock, flags);
	eu.low_word  = ioapic_read(ioapic_index, 0x10 + 2 * pin);
	eu.high_word = ioapic_read(ioapic_index, 0x11 + 2 * pin);
	spin_unlock_irqrestore(&ioapic_lock, flags);

	return eu.entry;
}

/**
 * Writes an IO APIC pin routing entry.
 *
 * When we write a new IO APIC routing entry, we need to write the high word
 * first. This is because the mask/enable bit is in the low word and we do not
 * want to enable the entry before it is fully populated.
 */
static void
ioapic_write_pin(
	unsigned int               ioapic_index,
	unsigned int               pin,
	struct IO_APIC_route_entry pin_config
)
{
	union ioapic_entry_union eu;
	unsigned long flags;

	spin_lock_irqsave(&ioapic_lock, flags);
	eu.entry = pin_config;
	ioapic_write(ioapic_index, 0x11 + 2 * pin, eu.high_word);
	ioapic_write(ioapic_index, 0x10 + 2 * pin, eu.low_word);
	spin_unlock_irqrestore(&ioapic_lock, flags);
}

/**
 * Masks (disables) an IO APIC input pin.
 */
static void
ioapic_mask_pin(
	unsigned int ioapic_index,
	unsigned int pin
)
{
	struct IO_APIC_route_entry pin_config =
					ioapic_read_pin(ioapic_index, pin);
	pin_config.mask = 1;
	ioapic_write_pin(ioapic_index, pin, pin_config);
}

/**
 * Unmasks (enables) an IO APIC input pin.
 */
static void
ioapic_unmask_pin(
	unsigned int ioapic_index,
	unsigned int pin
)
{
	struct IO_APIC_route_entry pin_config =
					ioapic_read_pin(ioapic_index, pin);
	pin_config.mask = 0;
	ioapic_write_pin(ioapic_index, pin, pin_config);
}

/**
 * Returns the number of input pins provided by the specified IO APIC.
 */
static unsigned int
ioapic_get_num_pins(unsigned int ioapic_index)
{
	union IO_APIC_reg_01 reg_01;

	reg_01.raw = ioapic_read(ioapic_index, 1);
	return reg_01.bits.entries + 1;
}

/**
 * This keeps track of the next vector to assign to an IO APIC pin.
 */
static unsigned int vector = IRQ0_VECTOR;

/**
 * Initializes the primary IO APIC (the one connected to the ISA IRQs).
 */
static void __init
ioapic_init_pins(
	unsigned int ioapic_index
)
{
	unsigned int pin;
	unsigned int num_pins = ioapic_get_num_pins(ioapic_index);
	struct IO_APIC_route_entry cfg;
	struct ioapic_info *ioapic = &ioapic_info[ioapic_index];
	struct ioapic_pin_info *pin_info;

	/* Mask (disable) all pins */
	for (pin = 0; pin < num_pins; pin++) {
		ioapic_mask_pin(ioapic_index, pin);
	}

	/* Configure all of the pins that we know about */
	for (pin = 0; pin < num_pins; pin++) {

		/* Make sure the pin is one we care about */
		pin_info = &ioapic->pin_info[pin];
		if (pin_info->num_srcs == 0)
			continue;
		if (pin_info->delivery_mode != ioapic_fixed)
			continue;

		/* Shared pins are allowed, but less than ideal */
		if (pin_info->num_srcs > 1) {
			printk(KERN_WARNING
				"I/O APIC %d pin %d is shared by %d devices.\n",
				ioapic->phys_id, pin, pin_info->num_srcs);
		}

		/* Assign an interrupt vector to the pin */
		if ((vector - IRQ0_VECTOR) <= pin) {
			vector = IRQ0_VECTOR + pin;
		}
		pin_info->os_assigned_vector = vector++;

		/* Program the pin */
		cfg = ioapic_read_pin(ioapic_index, pin);

		cfg.delivery_mode = pin_info->delivery_mode;
		cfg.dest_mode     = ioapic_physical_dest;
		cfg.polarity      = pin_info->polarity;
		cfg.trigger       = pin_info->trigger;
		cfg.dest          = (uint8_t) cpu_info[0].physical_id;
		cfg.vector        = pin_info->os_assigned_vector;

		ioapic_write_pin(ioapic_index, pin, cfg);
		ioapic_unmask_pin(ioapic_index, pin);
	}
}

/**
 * Creates a kernel mapping for all IO APICs in the system.
 */
void __init
ioapic_map(void)
{
	unsigned int i;
	const int name_size = 16;
	char *name;

#ifdef CONFIG_PC
	if (ioapic_num == 0) {
		printk(KERN_WARNING "Assuming 1 I/O APIC.\n");
		ioapic_num               = 1; /* assume there is one ioapic */
		ioapic_info[0].phys_id   = 1; /* and that its hw ID is 1 */
		ioapic_info[0].phys_addr = IOAPIC_DEFAULT_PHYS_BASE;
	}
#endif

	if (ioapic_num == 0)
		return;

	/*
	 * Allocate enough memory for one resource structure per detected IO
	 * APIC in the system. Memory for the resource name strings is tacked
	 * onto the end of the allocation (name_size*ioapic_num bytes).
	 */
	ioapic_resources = alloc_bootmem(ioapic_num *
	                                 (sizeof(struct resource) + name_size));
	name = ((char *)ioapic_resources) + ioapic_num*sizeof(struct resource);

	for (i = 0; i < ioapic_num; i++) {
		/* Reserve the physical memory used by the IO APIC */
		sprintf(name, "IO APIC %u", i);
		ioapic_resources[i].name  = name;
		ioapic_resources[i].flags = IORESOURCE_MEM | IORESOURCE_BUSY;
		ioapic_resources[i].start = ioapic_info[i].phys_addr;
		ioapic_resources[i].end   = ioapic_info[i].phys_addr + 4096 - 1;
		request_resource(&iomem_resource, &ioapic_resources[i]);
		name += name_size;

		/* Map the IO APIC into the kernel */
		set_fixmap_nocache(FIX_IO_APIC_BASE_0 + i,
		                   ioapic_info[i].phys_addr);

		printk(KERN_DEBUG
		       "IO APIC mapped to virtual address 0x%016lx\n",
		       __fix_to_virt(FIX_IO_APIC_BASE_0 + i)
		);
	}
}

/**
 * Initializes all IO APICs in the system.
 */
void __init
ioapic_init(void)
{
	if (ioapic_num == 0)
		return;

	for (int i = 0; i < ioapic_num; i++)
		ioapic_init_pins(i);

	ioapic_dump();
}

/**
 * Creates an ioapic_info structure for a newly encountered I/O APIC.
 */
struct ioapic_info *
ioapic_info_store(unsigned int phys_id, paddr_t phys_addr)
{
	if (ioapic_num >= MAX_IO_APICS) {
		printk(KERN_ERR "Max # of I/O APICs (%u) exceeded (found %u).\n",
			MAX_IO_APICS, ioapic_num);
		panic("Recompile kernel with bigger MAX_IO_APICS.\n");
	}

	struct ioapic_info *info = &ioapic_info[ioapic_num];
	ioapic_num++;

	info->phys_id   = phys_id;
	info->phys_addr = phys_addr;

	return info;
}

/**
 * Looks up the ioapic_info structure for the specified IO APIC.
 */
struct ioapic_info *
ioapic_info_lookup(unsigned int phys_id)
{
	for (int i = 0; i < ioapic_num; i++) {
		if (ioapic_info[i].phys_id == phys_id)
			return &ioapic_info[i];
	}
	return NULL;
}

/**
 * Determines the vector the OS assigned to a PCI device.
 */
int
ioapic_pcidev_vector(int bus, int slot, int pin)
{
	int i, j, k;
	struct ioapic_info *ioapic;
	struct ioapic_pin_info *pin_info;
	struct ioapic_src_info *src_info;

	for (i = 0; i < ioapic_num; i++) {
		ioapic = &ioapic_info[i];
		for (j = 0; j < MAX_IO_APIC_PINS; j++) {
			pin_info = &ioapic->pin_info[j];
			for (k = 0; k < pin_info->num_srcs; k++) {
				src_info = &pin_info->src_info[k];

				if (bus != src_info->bus_id)
					continue;

				/* The src bus_irq encodes the src slot */
				if (slot != ((src_info->bus_irq >> 2) & 0x1f))
					continue;

				/* The src bus_irq encodes the src pin */
				if (pin != (src_info->bus_irq & 0x3))
					continue;

				return (int)pin_info->os_assigned_vector;
			}
		}
	}

	printk(KERN_ERR "Failed to find vector for PCI device (%d.%d pin=%d)\n",
			bus, slot, pin);

	return -1;
}

/**
 * Dumps the current state of all IO APICs in the system.
 */
void __init
ioapic_dump(void)
{
	unsigned int ioapic_index, pin;
	union IO_APIC_reg_00 reg_00;
	union IO_APIC_reg_01 reg_01;
	union IO_APIC_reg_02 reg_02;
	struct IO_APIC_route_entry entry;
	unsigned long flags;

	for (ioapic_index = 0; ioapic_index < ioapic_num; ioapic_index++) {

		spin_lock_irqsave(&ioapic_lock, flags);
		reg_00.raw = ioapic_read(ioapic_index, 0);
		reg_01.raw = ioapic_read(ioapic_index, 1);
		if (reg_01.bits.version >= 0x10)
		    reg_02.raw = ioapic_read(ioapic_index, 2);
		spin_unlock_irqrestore(&ioapic_lock, flags);

		printk(KERN_DEBUG "Dump of IO APIC %u (physical id %u):\n",
		                  ioapic_index, ioapic_info[ioapic_index].phys_id);
		printk(KERN_DEBUG "  register #00: %08X\n", reg_00.raw);
		printk(KERN_DEBUG "    physical APIC id: %02u\n", reg_00.bits.ID);
		printk(KERN_DEBUG "  register #01: %08X\n", *(int *)&reg_01);
		printk(KERN_DEBUG "    max redirection entries: %04X\n", reg_01.bits.entries);
		printk(KERN_DEBUG "    PRQ implemented: %X\n", reg_01.bits.PRQ);
		printk(KERN_DEBUG "    IO APIC version: %04X\n", reg_01.bits.version);
		if (reg_01.bits.version >= 0x10) {
			printk(KERN_DEBUG "  register #02: %08X\n", reg_02.raw);
                	printk(KERN_DEBUG "    arbitration: %02X\n", reg_02.bits.arbitration);
		}

		printk(KERN_DEBUG "  Interrupt Redirection Table:\n");
		for (pin = 0; pin <= reg_01.bits.entries; pin++) {
			entry = ioapic_read_pin(ioapic_index, pin);
			printk(KERN_DEBUG
			       "    %02u: vector=%u dest=%03u mask=%1d "
			                 "trigger=%1d irr=%1d polarity=%1d\n",
			              pin, entry.vector, entry.dest, entry.mask,
			              entry.trigger, entry.irr, entry.polarity);
			printk(KERN_DEBUG
			       "        dest_mode=%1d delivery_mode=%1d "
			               "delivery_status=%1d\n",
			              entry.dest_mode, entry.delivery_mode,
			              entry.delivery_status);
		}
	}
}

