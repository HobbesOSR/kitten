/*
 *  acpi_numa.c - ACPI NUMA support
 *
 *  Copyright (C) 2002 Takayoshi Kochi <t-kochi@bq.jp.nec.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 */
#include <lwk/init.h>
#include <lwk/kernel.h>
#include <lwk/smp.h>
#include <lwk/string.h>
#include <lwk/types.h>
#include <lwk/errno.h>
#include <lwk/acpi.h>
#include <lwk/pmem.h>

#define ACPI_NUMA	0x80000000
#define _COMPONENT	ACPI_NUMA
ACPI_MODULE_NAME("numa")
#define PREFIX		"ACPI: "

int __initdata srat_rev;

void __init acpi_table_print_srat_entry(struct acpi_subtable_header * header)
{

	ACPI_FUNCTION_NAME("acpi_table_print_srat_entry");

	if (!header)
		return;

	switch (header->type) {

	case ACPI_SRAT_TYPE_CPU_AFFINITY:
		{
			struct acpi_srat_cpu_affinity *p =
			    container_of(header, struct acpi_srat_cpu_affinity, header);
			u32 proximity_domain = p->proximity_domain_lo;

			if (srat_rev >= 2) {
				proximity_domain |= p->proximity_domain_hi[0] << 8;
				proximity_domain |= p->proximity_domain_hi[1] << 16;
				proximity_domain |= p->proximity_domain_hi[2] << 24;
			}
			printk(KERN_INFO PREFIX
					  "SRAT Processor (id[0x%02x] eid[0x%02x]) in proximity domain %d %s\n",
					  p->apic_id, p->local_sapic_eid,
					  proximity_domain,
					  p->flags & ACPI_SRAT_CPU_ENABLED
					  ? "enabled" : "disabled");
		}
		break;

	case ACPI_SRAT_TYPE_MEMORY_AFFINITY:
		{
			struct acpi_srat_mem_affinity *p =
			    container_of(header, struct acpi_srat_mem_affinity, header);
			u32 proximity_domain = p->proximity_domain;

			if (srat_rev < 2)
				proximity_domain &= 0xff;
			printk(KERN_INFO PREFIX
					  "SRAT Memory (0x%016llx length 0x%016llx type 0x%x) in proximity domain %d %s%s\n",
					  p->base_address, p->length,
					  p->memory_type, proximity_domain,
					  p->flags & ACPI_SRAT_MEM_ENABLED
					  ? "enabled" : "disabled",
					  p->flags & ACPI_SRAT_MEM_HOT_PLUGGABLE
					  ? " hot-pluggable" : "");
		}
		break;

	case ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY:
		{
			struct acpi_srat_x2apic_cpu_affinity *p =
			    (struct acpi_srat_x2apic_cpu_affinity *)header;
			printk(KERN_INFO PREFIX
					  "SRAT Processor (x2apicid[0x%08x]) in"
					  " proximity domain %d %s\n",
					  p->apic_id,
					  p->proximity_domain,
					  (p->flags & ACPI_SRAT_CPU_ENABLED) ?
					  "enabled" : "disabled");
		}
		break;
	default:
		printk(KERN_WARNING PREFIX
		       "Found unsupported SRAT entry (type = 0x%x)\n",
		       header->type);
		break;
	}
}

static int __init acpi_parse_slit(struct acpi_table_header *table)
{
	//acpi_numa_slit_init((struct acpi_table_slit *)table);

	return 0;
}

#ifndef CONFIG_X86
void __init
acpi_numa_x2apic_affinity_init(struct acpi_srat_x2apic_cpu_affinity *pa)
{
	printk(KERN_WARNING PREFIX
	       "Found unsupported x2apic [0x%08x] SRAT entry\n", pa->apic_id);
	return;
}
#endif

static int __init
acpi_parse_x2apic_affinity(struct acpi_subtable_header *header,
			   const unsigned long end)
{
	struct acpi_srat_x2apic_cpu_affinity *processor_affinity;

	processor_affinity = (struct acpi_srat_x2apic_cpu_affinity *)header;
	if (!processor_affinity)
		return -EINVAL;

	acpi_table_print_srat_entry(header);

	/* let architecture-dependent part to do it */
	//acpi_numa_x2apic_affinity_init(processor_affinity);

	return 0;
}

static int __init
acpi_parse_processor_affinity(struct acpi_subtable_header * header,
			      const unsigned long end)
{
	struct acpi_srat_cpu_affinity *processor_affinity
		= container_of(header, struct acpi_srat_cpu_affinity, header);

	if (!processor_affinity)
		return -EINVAL;

	acpi_table_print_srat_entry(header);

	/* let architecture-dependent part to do it */
	//acpi_numa_processor_affinity_init(processor_affinity);

	return 0;
}

static int __init
acpi_parse_memory_affinity(struct acpi_subtable_header * header,
			   const unsigned long end)
{
	struct acpi_srat_mem_affinity *memory_affinity
		= container_of(header, struct acpi_srat_mem_affinity, header);

	if (!memory_affinity)
		return -EINVAL;

	acpi_table_print_srat_entry(header);

	/* let architecture-dependent part to do it */
	//acpi_numa_memory_affinity_init(memory_affinity);

	return 0;
}

static int __init
acpi_update_pmem(struct acpi_subtable_header * header,
		 const unsigned long _end)
{
	struct acpi_srat_mem_affinity *mem_affinity;
	paddr_t start, end;
	u32 numa_node;
	struct pmem_region query, result;
	int status;

	mem_affinity = container_of(header, struct acpi_srat_mem_affinity, header);
	if (!mem_affinity)
		return -EINVAL;

	start = mem_affinity->base_address;
	end   = mem_affinity->base_address + mem_affinity->length;

	numa_node = mem_affinity->proximity_domain;
	if (srat_rev < 2)
		numa_node &= 0xff;

	pmem_region_unset_all(&query);
	query.start = 0;
	query.end   = ULONG_MAX;

	while ((status = pmem_query(&query, &result)) == 0) {
		/* Setup for the next query */
		query.start = result.end;

		/* Handle no overlap cases */
		if ((result.end <= start) || (result.start >= end))
			continue;

		/* Handle head of pmem region not overlapping NUMA region */
		if (result.start < start)
			result.start = start;

		/* Handle tail of pmem region not overlapping NUMA region */
		if (result.end > end)
			result.end = end;
		
		/* Set the NUMA node for the matched region */
		result.numa_node_is_set = true;
		result.numa_node        = numa_node;

		printk(KERN_INFO PREFIX
			"Trying to update [0x%llx, 0x%llx) to be on numa_node %u...\n",
			(unsigned long long) result.start, (unsigned long long)result.end, numa_node);

		if ((status = pmem_update(&result)) != 0)
			panic("pmem_update failed! status=%d\n", status);
		
		printk(KERN_INFO PREFIX "    Success!!!\n");
	}

	return 0;
}

int __init acpi_parse_srat(struct acpi_table_header *table)
{
	if (!table)
		return -EINVAL;

	srat_rev = table->revision;

	return 0;
}

int __init
acpi_table_parse_srat(enum acpi_srat_entry_id id,
		      acpi_madt_entry_handler handler, unsigned int max_entries)
{
	return acpi_table_parse_entries(ACPI_SIG_SRAT,
					sizeof(struct acpi_table_srat), id,
					handler, max_entries);
}

int __init acpi_numa_init(void)
{
	/* SRAT: Static Resource Affinity Table */
	if (!acpi_table_parse(ACPI_SIG_SRAT, acpi_parse_srat)) {
		printk(KERN_INFO PREFIX
			"Parsing SRAT_TYPE_X2APIC_CPU_AFFINITY table...\n");
		acpi_table_parse_srat(ACPI_SRAT_TYPE_X2APIC_CPU_AFFINITY,
				           acpi_parse_x2apic_affinity, NR_CPUS);
		printk(KERN_INFO PREFIX "DONE.\n");
		printk(KERN_INFO PREFIX "\n");

		printk(KERN_INFO PREFIX
			"Parsing SRAT_PROCESSOR_AFFINITY table...\n");
		acpi_table_parse_srat(ACPI_SRAT_PROCESSOR_AFFINITY,
					       acpi_parse_processor_affinity,
					       NR_CPUS);
		printk(KERN_INFO PREFIX "DONE.\n");
		printk(KERN_INFO PREFIX "\n");

		printk(KERN_INFO PREFIX
			"Parsing SRAT_MEMORY_AFFINITY table...\n");
		acpi_table_parse_srat(ACPI_SRAT_MEMORY_AFFINITY,
				acpi_parse_memory_affinity, NR_CPUS * 2);
		printk(KERN_INFO PREFIX "DONE.\n");
		printk(KERN_INFO PREFIX "\n");
	}

	/* SLIT: System Locality Information Table */
	acpi_table_parse(ACPI_SIG_SLIT, acpi_parse_slit);

	return 0;
}

void __init acpi_set_pmem_numa_info(void)
{
	if (!acpi_table_parse(ACPI_SIG_SRAT, acpi_parse_srat)) {
		/* Parse the Static Resource Affinity Table for NUMA info */
		acpi_table_parse_srat(ACPI_SRAT_MEMORY_AFFINITY,
				      acpi_update_pmem, NR_CPUS * 2);
	}
}
