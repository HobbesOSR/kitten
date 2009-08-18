#ifndef _ASM_X86_IO_APIC_H
#define _ASM_X86_IO_APIC_H

#include <arch/io_apic.h>

/*
 * Kitten always uses the I/O APIC for interrupt routing, so disable
 * automatic assignment of PCI IRQ's.
 */
#define io_apic_assign_pci_irqs 1

extern int IO_APIC_get_PCI_irq_vector(int bus, int slot, int pin);

#endif
