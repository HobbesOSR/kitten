#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <arch/io.h>
#include <arch/pci/pcicfg.h>


#define PCICFG_ADDR_PORT	0xCF8
#define PCICFG_DATA_PORT	0xCFC
#define PCICFG_ADDR_ENABLE_BIT	(1 << 31)


static DEFINE_SPINLOCK(pcicfg_lock);


static uint32_t
calc_addr(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	unsigned int	reg
)
{
	return (PCICFG_ADDR_ENABLE_BIT
		| (bus   << 16)
		| (slot  << 11)
		| (func  <<  8)
		| (reg & ~0x3)  /* last two bits must be 0 */
	);
}


uint32_t
arch_pcicfg_read(
	unsigned int	bus,
	unsigned int	slot,
	unsigned int	func,
	unsigned int	reg,
	unsigned int	width
)
{
	printk("Unhandled function %s",__FUNCTION__);
#if 0
	uint32_t addr = calc_addr(bus, slot, func, reg);
	uint32_t value = 0;
	unsigned long irqstate;

	spin_lock_irqsave(&pcicfg_lock, irqstate);

	/* Specify the PCI config space address to read */
	outl(addr, PCICFG_ADDR_PORT);

	/* Read the data requested from PCI config space */
	switch (width) {
		case 1: value = inb(PCICFG_DATA_PORT + (reg & 0x3)); break;
		case 2: value = inw(PCICFG_DATA_PORT + (reg & 0x3)); break;
		case 4: value = inl(PCICFG_DATA_PORT); break;
	}

	spin_unlock_irqrestore(&pcicfg_lock, irqstate);

	return value;
#endif
	return 0;
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
	printk("Unhandled function %s",__FUNCTION__);
#if 0
	uint32_t addr = calc_addr(bus, slot, func, reg);
	unsigned long irqstate;

	spin_lock_irqsave(&pcicfg_lock, irqstate);

	/* Specify the PCI config space address to write */
	outl(addr, PCICFG_ADDR_PORT);

	/* Write the value requested to PCI config space */
	switch (width) {
		case 1: outb((uint8_t)  value, PCICFG_DATA_PORT + (reg & 0x3)); break;
		case 2: outw((uint16_t) value, PCICFG_DATA_PORT + (reg & 0x3)); break;
		case 4: outl(           value, PCICFG_DATA_PORT); break;
	}

	spin_unlock_irqrestore(&pcicfg_lock, irqstate);
#endif
}
