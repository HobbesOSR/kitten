#ifndef __ASM_IO_APIC_H
#define __ASM_IO_APIC_H

#include <lwk/spinlock.h>
#include <arch/types.h>
#include <arch/mpspec.h>

struct ioapic_src_info {
	unsigned int		bus_id;
	unsigned int		bus_irq;
};

struct ioapic_pin_info {
	unsigned int		num_srcs;
	struct ioapic_src_info	src_info[MAX_IO_APIC_SRCS];
	unsigned int		delivery_mode;
	unsigned int		polarity;
	unsigned int		trigger;
	unsigned int		os_assigned_vector;
};

struct ioapic_info {
	unsigned int		phys_id;
	paddr_t			phys_addr;
	struct ioapic_pin_info	pin_info[MAX_IO_APIC_PINS];
};

extern unsigned int ioapic_num;
extern struct ioapic_info ioapic_info[MAX_IO_APICS];

extern void __init ioapic_map(void);
extern void __init ioapic_init(void);
extern struct ioapic_info *ioapic_info_store(unsigned int phys_id, paddr_t phys_addr);
extern struct ioapic_info *ioapic_info_lookup(unsigned int phys_id);
extern int ioapic_pcidev_vector(int bus, int slot, int pin);
extern int ioapic_mask_vector(unsigned int vector);
extern int ioapic_unmask_vector(unsigned int vector);
extern int ioapic_set_trigger_for_vector(unsigned int vector, unsigned int trigger);
extern void ioapic_dump(void);

/*
 * Intel IO-APIC support for SMP and UP systems.
 *
 * Copyright (C) 1997, 1998, 1999, 2000 Ingo Molnar
 */

static inline int use_pci_vector(void)	{return 1;}
static inline void disable_edge_ioapic_vector(unsigned int vector) { }
static inline void mask_and_ack_level_ioapic_vector(unsigned int vector) { }
static inline void end_edge_ioapic_vector (unsigned int vector) { }
#define startup_level_ioapic	startup_level_ioapic_vector
#define shutdown_level_ioapic	mask_IO_APIC_vector
#define enable_level_ioapic	unmask_IO_APIC_vector
#define disable_level_ioapic	mask_IO_APIC_vector
#define mask_and_ack_level_ioapic mask_and_ack_level_ioapic_vector
#define end_level_ioapic	end_level_ioapic_vector
#define set_ioapic_affinity	set_ioapic_affinity_vector

#define startup_edge_ioapic 	startup_edge_ioapic_vector
#define shutdown_edge_ioapic 	disable_edge_ioapic_vector
#define enable_edge_ioapic 	unmask_IO_APIC_vector
#define disable_edge_ioapic 	disable_edge_ioapic_vector
#define ack_edge_ioapic 	ack_edge_ioapic_vector
#define end_edge_ioapic 	end_edge_ioapic_vector

#define APIC_MISMATCH_DEBUG

#define IO_APIC_BASE(idx) \
		((volatile int *)(__fix_to_virt(FIX_IO_APIC_BASE_0 + idx) \
		+ (ioapic_info[idx].phys_addr & ~PAGE_MASK)))

/*
 * The structure of the IO-APIC:
 */
union IO_APIC_reg_00 {
	u32	raw;
	struct {
		u32	__reserved_2	: 14,
			LTS		:  1,
			delivery_type	:  1,
			__reserved_1	:  8,
			ID		:  8;
	} __attribute__ ((packed)) bits;
};

union IO_APIC_reg_01 {
	u32	raw;
	struct {
		u32	version		:  8,
		__reserved_2	:  7,
		PRQ		:  1,
		entries		:  8,
		__reserved_1	:  8;
	} __attribute__ ((packed)) bits;
};

union IO_APIC_reg_02 {
	u32	raw;
	struct {
		u32	__reserved_2	: 24,
		arbitration	:  4,
		__reserved_1	:  4;
	} __attribute__ ((packed)) bits;
};

union IO_APIC_reg_03 {
	u32	raw;
	struct {
		u32	boot_DT		:  1,
			__reserved_1	: 31;
	} __attribute__ ((packed)) bits;
};

enum ioapic_trigger_modes {
	ioapic_edge_sensitive  = 0,
	ioapic_level_sensitive = 1
};

enum ioapic_pin_polarities {
	ioapic_active_high = 0,
	ioapic_active_low  = 1
};

enum ioapic_destination_modes {
	ioapic_physical_dest = 0,
	ioapic_logical_dest  = 1
};

enum ioapic_delivery_modes {
	ioapic_fixed           = 0,
	ioapic_lowest_priority = 1,
	ioapic_SMI             = 2,
	ioapic_NMI             = 4,
	ioapic_INIT            = 5,
	ioapic_ExtINT          = 7
};

struct IO_APIC_route_entry {
	__u32	vector		:  8,
		delivery_mode	:  3,	/* 000: FIXED
					 * 001: lowest prio
					 * 111: ExtINT
					 */
		dest_mode	:  1,	/* 0: physical, 1: logical */
		delivery_status	:  1,
		polarity	:  1,
		irr		:  1,
		trigger		:  1,	/* 0: edge, 1: level */
		mask		:  1,	/* 0: enabled, 1: disabled */
		__reserved_2	: 15;

	__u32	__reserved_3	: 24,
		dest		:  8;
} __attribute__ ((packed));

static inline unsigned int io_apic_read(unsigned int apic, unsigned int reg)
{
	*IO_APIC_BASE(apic) = reg;
	return *(IO_APIC_BASE(apic)+4);
}

static inline void io_apic_write(unsigned int apic, unsigned int reg, unsigned int value)
{
	*IO_APIC_BASE(apic) = reg;
	*(IO_APIC_BASE(apic)+4) = value;
}

/*
 * Re-write a value: to be used for read-modify-write
 * cycles where the read already set up the index register.
 */
static inline void io_apic_modify(unsigned int apic, unsigned int value)
{
	*(IO_APIC_BASE(apic)+4) = value;
}

/*
 * Synchronize the IO-APIC and the CPU by doing
 * a dummy read from the IO-APIC
 */
static inline void io_apic_sync(unsigned int apic)
{
	(void) *(IO_APIC_BASE(apic)+4);
}

#endif
