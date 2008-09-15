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
 * Array containing the IDs of the IO APICs in the system.
 * The array is indexed by ioapic_index.
 */
unsigned int ioapic_id[MAX_IO_APICS];

/**
 * Addresses of the IO APICs in the system.
 * The array is indexed by ioapic_index.
 */
unsigned long ioapic_phys_addr[MAX_IO_APICS];

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
		 + (ioapic_phys_addr[ioapic_index] & ~PAGE_MASK);
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
 * Initializes the primary IO APIC (the one connected to the ISA IRQs).
 */
static void __init
ioapic_init_primary(
	unsigned int ioapic_index
)
{
	unsigned int pin;
	unsigned int num_pins = ioapic_get_num_pins(ioapic_index);
	struct IO_APIC_route_entry cfg;

	if (num_pins != 24)
		panic("Expected IOAPIC to have 24 pins, has %u.", num_pins);

	/* Mask (disable) all pins */
	for (pin = 0; pin < num_pins; pin++) {
		ioapic_mask_pin(ioapic_index, pin);
	}

	/*
	 * Configure ISA IRQs.
	 * (Assuming pins [1,15] are the standard ISA IRQs)
	 * (Assuming pin 2 is hooked to the timer interrupt)
	 * (Assuming pin 0 is hooked to the old i8259 PIC... don't use it)
	 */
	for (pin = 1; pin <= 15; pin++) {
		cfg = ioapic_read_pin(ioapic_index, pin);

		cfg.delivery_mode = ioapic_fixed;
		cfg.dest_mode     = ioapic_physical_dest;
		cfg.polarity      = (pin == 8)
		                        ? ioapic_active_low
		                        : ioapic_active_high;
		cfg.trigger       = ioapic_edge_sensitive;
		cfg.dest          = (uint8_t) cpu_info[0].physical_id;
		cfg.vector        = IRQ0_VECTOR + pin;

		ioapic_write_pin(ioapic_index, pin, cfg);
	}

	/*
	 * Configure PCI IRQs.
	 * (Assuming pins [16,19] are PCI INTA, INTB, INTC, and INTD)
	 */
	for (pin = 16; pin <= 19; pin++) {
		cfg = ioapic_read_pin(ioapic_index, pin);

		cfg.delivery_mode = ioapic_fixed;
		cfg.dest_mode     = ioapic_physical_dest;
		cfg.polarity      = ioapic_active_low;
		cfg.trigger       = ioapic_level_sensitive;
		cfg.dest          = (uint8_t) cpu_info[0].physical_id;
		cfg.vector        = IRQ0_VECTOR + pin;

		ioapic_write_pin(ioapic_index, pin, cfg);
	} 

	/* Unmask (enable) all of the pins that have been configured */
	for (pin = 1; pin < 19; pin++) {
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
		ioapic_resources[i].start = ioapic_phys_addr[i];
		ioapic_resources[i].end   = ioapic_phys_addr[i] + 4096 - 1;
		request_resource(&iomem_resource, &ioapic_resources[i]);
		name += name_size;

		/* Map the IO APIC into the kernel */
		set_fixmap_nocache(FIX_IO_APIC_BASE_0 + i, ioapic_phys_addr[i]);

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

/* TODO: FIX THIS... NEED TO PARSE MPTABLE OR SOMETHING ELSE */
#ifdef CONFIG_PC
	/* TODO: For now, only initializes the first one. */
	ioapic_init_primary(0);
	ioapic_dump();
#endif
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
		                  ioapic_index, ioapic_id[ioapic_index]);
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

