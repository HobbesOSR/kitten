/*
 * acpi.h - ACPI Interface
 *
 * Copyright (C) 2001 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef _LWK_ACPI_H
#define _LWK_ACPI_H

#ifndef _LINUX
#define _LINUX
#endif

#include <lwk/list.h>
#include <arch/io.h>

#include <acpi/acpi.h>
#include <arch/acpi.h>

enum acpi_irq_model_id {
	ACPI_IRQ_MODEL_PIC = 0,
	ACPI_IRQ_MODEL_IOAPIC,
	ACPI_IRQ_MODEL_IOSAPIC,
	ACPI_IRQ_MODEL_PLATFORM,
	ACPI_IRQ_MODEL_COUNT
};

extern enum acpi_irq_model_id	acpi_irq_model;

enum acpi_interrupt_id {
	ACPI_INTERRUPT_PMI	= 1,
	ACPI_INTERRUPT_INIT,
	ACPI_INTERRUPT_CPEI,
	ACPI_INTERRUPT_COUNT
};

#define	ACPI_SPACE_MEM		0

enum acpi_srat_entry_id {
	ACPI_SRAT_PROCESSOR_AFFINITY = 0,
	ACPI_SRAT_MEMORY_AFFINITY,
	ACPI_SRAT_ENTRY_COUNT
};

enum acpi_address_range_id {
	ACPI_ADDRESS_RANGE_MEMORY = 1,
	ACPI_ADDRESS_RANGE_RESERVED = 2,
	ACPI_ADDRESS_RANGE_ACPI = 3,
	ACPI_ADDRESS_RANGE_NVS	= 4,
	ACPI_ADDRESS_RANGE_COUNT
};

typedef int (*acpi_madt_entry_handler) (struct acpi_subtable_header *header,
		        const unsigned long end);

/* Table Handlers */

typedef int (*acpi_table_handler) (struct acpi_table_header *table);

typedef int (*acpi_table_entry_handler) (struct acpi_subtable_header *header, const unsigned long end);

#define BAD_MADT_ENTRY(entry, end) (					    \
		(!entry) || (unsigned long)entry + sizeof(*entry) > end ||  \
		((struct acpi_subtable_header *)entry)->length < sizeof(*entry))

void * __acpi_map_table (unsigned long phys_addr, unsigned long size);
void __acpi_unmap_table(void *map, unsigned long size);
int early_acpi_boot_init(void);
int acpi_boot_init (void);
void acpi_init (void);
int acpi_mps_check (void);
int acpi_parse_madt (void);
int acpi_numa_init (void);

int acpi_table_init (void);
int acpi_table_parse (char *id, acpi_table_handler handler);
int __init acpi_table_parse_entries(char *id, unsigned long table_size,
	int entry_id, acpi_table_entry_handler handler, unsigned int max_entries);
int acpi_table_parse_madt (enum acpi_madt_type id, acpi_table_entry_handler handler, unsigned int max_entries);
int acpi_parse_mcfg (struct acpi_table_header *header);
void acpi_table_print_madt_entry (struct acpi_subtable_header *madt);

/* the following four functions are architecture-dependent */
void acpi_numa_slit_init (struct acpi_table_slit *slit);
void acpi_numa_processor_affinity_init (struct acpi_srat_cpu_affinity *pa);
void acpi_numa_x2apic_affinity_init(struct acpi_srat_x2apic_cpu_affinity *pa);
void acpi_numa_memory_affinity_init (struct acpi_srat_mem_affinity *ma);
void acpi_numa_arch_fixup(void);
void acpi_set_pmem_numa_info(void);

int acpi_register_ioapic(acpi_handle handle, u64 phys_addr, u32 gsi_base);
int acpi_unregister_ioapic(acpi_handle handle, u32 gsi_base);
void acpi_irq_stats_init(void);
extern u32 acpi_irq_handled;
extern u32 acpi_irq_not_handled;

extern int sbf_port;
extern unsigned long acpi_realmode_flags;

#ifdef CONFIG_X86_IO_APIC
extern int acpi_get_override_irq(u32 gsi, int *trigger, int *polarity);
#else
#define acpi_get_override_irq(gsi, trigger, polarity) (-1)
#endif
/*
 * This function undoes the effect of one call to acpi_register_gsi().
 * If this matches the last registration, any IRQ resources for gsi
 * are freed.
 */
void acpi_unregister_gsi (u32 gsi);

struct pci_dev;

int acpi_pci_irq_enable (struct pci_dev *dev);
void acpi_penalize_isa_irq(int irq, int active);

void acpi_pci_irq_disable (struct pci_dev *dev);

struct acpi_pci_driver {
	struct acpi_pci_driver *next;
	int (*add)(acpi_handle handle);
	void (*remove)(acpi_handle handle);
};

int acpi_pci_register_driver(struct acpi_pci_driver *driver);
void acpi_pci_unregister_driver(struct acpi_pci_driver *driver);

extern int ec_read(u8 addr, u8 *val);
extern int ec_write(u8 addr, u8 val);
extern int ec_transaction(u8 command,
                          const u8 *wdata, unsigned wdata_len,
                          u8 *rdata, unsigned rdata_len);

#define ACPI_VIDEO_OUTPUT_SWITCHING			0x0001
#define ACPI_VIDEO_DEVICE_POSTING			0x0002
#define ACPI_VIDEO_ROM_AVAILABLE			0x0004
#define ACPI_VIDEO_BACKLIGHT				0x0008
#define ACPI_VIDEO_BACKLIGHT_FORCE_VENDOR		0x0010
#define ACPI_VIDEO_BACKLIGHT_FORCE_VIDEO		0x0020
#define ACPI_VIDEO_OUTPUT_SWITCHING_FORCE_VENDOR	0x0040
#define ACPI_VIDEO_OUTPUT_SWITCHING_FORCE_VIDEO		0x0080
#define ACPI_VIDEO_BACKLIGHT_DMI_VENDOR			0x0100
#define ACPI_VIDEO_BACKLIGHT_DMI_VIDEO			0x0200
#define ACPI_VIDEO_OUTPUT_SWITCHING_DMI_VENDOR		0x0400
#define ACPI_VIDEO_OUTPUT_SWITCHING_DMI_VIDEO		0x0800

int acpi_get_pxm(acpi_handle handle);
int acpi_get_node(acpi_handle *handle);
extern int acpi_paddr_to_node(u64 start_addr, u64 size);

extern int pnpacpi_disabled;

#define PXM_INVAL	(-1)
#define NID_INVAL	(-1)

struct acpi_osc_context {
	char *uuid_str; /* uuid string */
	int rev;
	struct acpi_buffer cap; /* arg2/arg3 */
	struct acpi_buffer ret; /* free by caller if success */
};

#define OSC_QUERY_TYPE			0
#define OSC_SUPPORT_TYPE 		1
#define OSC_CONTROL_TYPE		2

/* _OSC DW0 Definition */
#define OSC_QUERY_ENABLE		1
#define OSC_REQUEST_ERROR		2
#define OSC_INVALID_UUID_ERROR		4
#define OSC_INVALID_REVISION_ERROR	8
#define OSC_CAPABILITIES_MASK_ERROR	16

acpi_status acpi_run_osc(acpi_handle handle, struct acpi_osc_context *context);

/* platform-wide _OSC bits */
#define OSC_SB_PAD_SUPPORT		1
#define OSC_SB_PPC_OST_SUPPORT		2
#define OSC_SB_PR3_SUPPORT		4
#define OSC_SB_CPUHP_OST_SUPPORT	8
#define OSC_SB_APEI_SUPPORT		16

/* PCI defined _OSC bits */
/* _OSC DW1 Definition (OS Support Fields) */
#define OSC_EXT_PCI_CONFIG_SUPPORT		1
#define OSC_ACTIVE_STATE_PWR_SUPPORT 		2
#define OSC_CLOCK_PWR_CAPABILITY_SUPPORT	4
#define OSC_PCI_SEGMENT_GROUPS_SUPPORT		8
#define OSC_MSI_SUPPORT				16
#define OSC_PCI_SUPPORT_MASKS			0x1f

/* _OSC DW1 Definition (OS Control Fields) */
#define OSC_PCI_EXPRESS_NATIVE_HP_CONTROL	1
#define OSC_SHPC_NATIVE_HP_CONTROL 		2
#define OSC_PCI_EXPRESS_PME_CONTROL		4
#define OSC_PCI_EXPRESS_AER_CONTROL		8
#define OSC_PCI_EXPRESS_CAP_STRUCTURE_CONTROL	16

#define OSC_PCI_CONTROL_MASKS 	(OSC_PCI_EXPRESS_NATIVE_HP_CONTROL | 	\
				OSC_SHPC_NATIVE_HP_CONTROL | 		\
				OSC_PCI_EXPRESS_PME_CONTROL |		\
				OSC_PCI_EXPRESS_AER_CONTROL |		\
				OSC_PCI_EXPRESS_CAP_STRUCTURE_CONTROL)
extern acpi_status acpi_pci_osc_control_set(acpi_handle handle,
					     u32 *mask, u32 req);
extern void acpi_early_init(void);

static inline void __iomem *acpi_os_ioremap(acpi_physical_address phys,
					    acpi_size size)
{
	return ioremap(phys, size);
}

#endif	/*_LWK_ACPI_H*/
