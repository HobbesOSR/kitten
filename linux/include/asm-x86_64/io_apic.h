#ifndef _ASM_X86_IO_APIC_H
#define _ASM_X86_IO_APIC_H

#include <arch/io_apic.h>

/**
 * LWK always uses the I/O APIC for interrupt routing,
 * so disable automatic assignment of PCI IRQ's.
 */
#define io_apic_assign_pci_irqs 1

/**
 * LWK uses ioapic_num to count the number of IO APICs,
 * Linux uses nr_ioapics.
 */
#define nr_ioapics ioapic_num

static inline int
IO_APIC_get_PCI_irq_vector(int bus, int slot, int pin)
{
	return ioapic_pcidev_vector(bus, slot, pin);
}

#endif
