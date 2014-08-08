#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <arch/io.h>
#include <arch/types.h>
#include <arch/pci/pcicfg.h>


#define PCI_DEVFN(slot, func) ((((slot) & 0x1f) << 3) | ((func) & 0x07))

static DEFINE_SPINLOCK(pcicfg_lock);

/*
 * Functions for accessing PCI base (first 256 bytes) and extended
 * (4096 bytes per PCI function) configuration space with type 1
 * accesses.
 * 
 * Mechanism #2 is unsupported
 * 
 * For now we are going to try to use the AMD PCI access extensions 
 *   for the extended regions. This is a hack, and might not work on
 *   non-AMD platforms. 
 * 
 * TODO: Add mmconfig (MMCFG) PCI support via ACPI for extended config access
 */

#define PCI_CONF1_ADDRESS(bus, devfn, reg) \
	(0x80000000 | ((reg & 0xF00) << 16) | (bus << 16) \
	| (devfn << 8) | (reg & 0xFC))


#if 0

static int __init pci_check_type1(void)
{
	unsigned long flags;
	unsigned int tmp;
	int works = 0;

	local_irq_save(flags);

	outb(0x01, 0xCFB);
	tmp = inl(0xCF8);
	outl(0x80000000, 0xCF8);
	if (inl(0xCF8) == 0x80000000) {
		works = 1;
	}
	outl(tmp, 0xCF8);
	local_irq_restore(flags);

	return works;
}

#endif

int 
raw_pci_read(unsigned int   seg, 
		unsigned int   bus,
 		unsigned int   devfn, 
		int            reg,
		int            len,
  		u32          * value)
{
	unsigned long flags;

	if ((bus > 255) || (devfn > 255) || (reg > 4095)) {
		*value = -1;
		return -EINVAL;
	}

	spin_lock_irqsave(&pcicfg_lock, flags);

	outl(PCI_CONF1_ADDRESS(bus, devfn, reg), 0xCF8);

	switch (len) {
	case 1:
		*value = inb(0xCFC + (reg & 3));
		break;
	case 2:
		*value = inw(0xCFC + (reg & 2));
		break;
	case 4:
		*value = inl(0xCFC);
		break;
	}

	spin_unlock_irqrestore(&pcicfg_lock, flags);

	return 0;
}

int 
raw_pci_write(unsigned int   seg, 
		 unsigned int   bus,
		 unsigned int   devfn,
		 int            reg, 
		 int            len, 
		 u32            value)
{
	unsigned long flags;

	if ((bus > 255) || (devfn > 255) || (reg > 4095))
		return -EINVAL;

	spin_lock_irqsave(&pcicfg_lock, flags);

	outl(PCI_CONF1_ADDRESS(bus, devfn, reg), 0xCF8);

	switch (len) {
	case 1:
		outb((u8)value, 0xCFC + (reg & 3));
		break;
	case 2:
		outw((u16)value, 0xCFC + (reg & 2));
		break;
	case 4:
		outl((u32)value, 0xCFC);
		break;
	}

	spin_unlock_irqrestore(&pcicfg_lock, flags);

	return 0;
}

#undef PCI_CONF1_ADDRESS



uint32_t
arch_pcicfg_read(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	unsigned int	reg,
	unsigned int	width
)
{
    u32 val = 0;

    raw_pci_read(0, bus, PCI_DEVFN(slot, func), reg, width, &val);

    return val;
}


void
arch_pcicfg_write(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	unsigned int	reg,
	unsigned int	width,
	uint32_t	value
)
{
    raw_pci_write(0, bus, PCI_DEVFN(slot, func), reg, width, value);
}

