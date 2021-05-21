

#include <lwk/smp.h>
#include <lwk/init.h>
#include <lwk/bootmem.h>
#include <lwk/cpuinfo.h>
#include <lwk/params.h>
#include <lwk/cpumask.h>
#include <lwk/acpi.h>

#include <arch/io.h>
#include <arch/mpspec.h>
#include <arch/proto.h>
#include <arch/of.h>
#include <arch/cputype.h>
#include <arch/smp_plat.h>
#include <arch/acpi.h>
#include <arch/cpu_ops.h>



unsigned int nr_cpu_ids = NR_CPUS;

/**
 * The number of CPUs in the system.
 */
//unsigned int __initdata num_cpus = 0;

/**
 * Map of all CPUs present.
 * Bits set represent physical CPU IDs present.
 */
physid_mask_t phys_cpu_present_map = PHYSID_MASK_NONE;

static bool bootcpu_valid __initdata;
static unsigned int cpu_count = 1;


DEFINE_PER_CPU_READ_MOSTLY(int, cpu_number);
EXPORT_PER_CPU_SYMBOL(cpu_number);


static struct acpi_madt_generic_interrupt cpu_madt_gicc[NR_CPUS];

struct acpi_madt_generic_interrupt *acpi_cpu_get_madt_gicc(int cpu)
{
	return &cpu_madt_gicc[cpu];
}





/*
 * Duplicate MPIDRs are a recipe for disaster. Scan all initialized
 * entries and check for duplicates. If any is found just ignore the
 * cpu. cpu_logical_map was initialized to INVALID_HWID to avoid
 * matching valid MPIDR values.
 */
static bool __init is_mpidr_duplicate(unsigned int cpu, u64 hwid)
{
	unsigned int i;

	for (i = 1; (i < cpu) && (i < NR_CPUS); i++)
		if (cpu_logical_map(i) == hwid)
			return true;
	return false;
}


/*
 * Initialize cpu operations for a logical cpu and
 * set it in the possible mask on success
 */
static int __init smp_cpu_setup(int cpu)
{
	if (cpu_read_ops(cpu))
		return -ENODEV;

	if (cpu_ops[cpu]->cpu_init(cpu))
		return -ENODEV;

	set_cpu_possible(cpu, true);

	return 0;
}

#if 0

/*
 * acpi_map_gic_cpu_interface - parse processor MADT entry
 *
 * Carry out sanity checks on MADT processor entry and initialize
 * cpu_logical_map on success
 */
static void __init
acpi_map_gic_cpu_interface(struct acpi_madt_generic_interrupt *processor)
{
	u64 hwid = processor->arm_mpidr;

	if (!(processor->flags & ACPI_MADT_ENABLED)) {
		printk("skipping disabled CPU entry with 0x%llx MPIDR\n", hwid);
		return;
	}

	if (hwid & ~MPIDR_HWID_BITMASK || hwid == INVALID_HWID) {
		printk("skipping CPU entry with invalid MPIDR 0x%llx\n", hwid);
		return;
	}

	if (is_mpidr_duplicate(cpu_count, hwid)) {
		printk("duplicate CPU MPIDR 0x%llx in MADT\n", hwid);
		return;
	}

	/* Check if GICC structure of boot CPU is available in the MADT */
	if (cpu_logical_map(0) == hwid) {
		if (bootcpu_valid) {
			printk("duplicate boot CPU MPIDR: 0x%llx in MADT\n",
			       hwid);
			return;
		}
		bootcpu_valid = true;
		cpu_madt_gicc[0] = *processor;
		early_map_cpu_to_node(0, acpi_numa_get_nid(0, hwid));
		return;
	}

	if (cpu_count >= NR_CPUS)
		return;

	/* map the logical cpu id to cpu MPIDR */
	cpu_logical_map(cpu_count) = hwid;

	cpu_madt_gicc[cpu_count] = *processor;

	/*
	 * Set-up the ACPI parking protocol cpu entries
	 * while initializing the cpu_logical_map to
	 * avoid parsing MADT entries multiple times for
	 * nothing (ie a valid cpu_logical_map entry should
	 * contain a valid parking protocol data set to
	 * initialize the cpu if the parking protocol is
	 * the only available enable method).
	 */
	acpi_set_mailbox_entry(cpu_count, processor);

	early_map_cpu_to_node(cpu_count, acpi_numa_get_nid(cpu_count, hwid));

	cpu_count++;
}

static int __init
acpi_parse_gic_cpu_interface(struct acpi_subtable_header *header,
			     const unsigned long end)
{
	struct acpi_madt_generic_interrupt *processor;

	processor = (struct acpi_madt_generic_interrupt *)header;
	if (BAD_MADT_GICC_ENTRY(processor, end))
		return -EINVAL;

	acpi_table_print_madt_entry(header);

	acpi_map_gic_cpu_interface(processor);

	return 0;
}
#endif

static u64 __init of_get_cpu_mpidr(struct device_node *dn)
{
	const __be32 *cell;
	u64 hwid;

	/*
	 * A cpu node with missing "reg" property is
	 * considered invalid to build a cpu_logical_map
	 * entry.
	 */
	cell = of_get_property(dn, "reg", NULL);
	if (!cell) {
		pr_err("%pOF: missing reg property\n", dn);
		return INVALID_HWID;
	}

	hwid = of_read_number(cell, of_n_addr_cells(dn));
	/*
	 * Non affinity bits must be set to 0 in the DT
	 */
	if (hwid & ~MPIDR_HWID_BITMASK) {
		pr_err("%pOF: invalid reg property\n", dn);
		return INVALID_HWID;
	}
	return hwid;
}


/*
 * Enumerate the possible CPU set from the device tree and build the
 * cpu logical map array containing MPIDR values related to logical
 * cpus. Assumes that cpu_logical_map(0) has already been initialized.
 */
static void __init of_parse_and_init_cpus(void)
{
	struct device_node *dn;


	printk("Parsing FDT for CPUs\n");

	for_each_node_by_type(dn, "cpu") {
		u64 hwid = of_get_cpu_mpidr(dn);

		printk("Found CPU ID: %p\n", (void *)hwid);

		if (hwid == INVALID_HWID)
			goto next;

		if (is_mpidr_duplicate(cpu_count, hwid)) {
			printk("%pOF: duplicate cpu reg properties in the DT\n",
				dn);
			goto next;
		}


		printk("cpu_logical_map(0) = %lld\n", cpu_logical_map(0));

		/*
		 * The numbering scheme requires that the boot CPU
		 * must be assigned logical id 0. Record it so that
		 * the logical map built from DT is validated and can
		 * be used.
		 */
		if (hwid == cpu_logical_map(0)) {
			if (bootcpu_valid) {
				printk("%pOF: duplicate boot cpu reg property in DT\n",
					dn);
				goto next;
			}

			bootcpu_valid = true;
			// early_map_cpu_to_node(0, of_node_to_nid(dn));
			early_write_pda(0, nodenumber, of_node_to_nid(dn));

			/*
			 * cpu_logical_map has already been
			 * initialized and the boot cpu doesn't need
			 * the enable-method so continue without
			 * incrementing cpu.
			 */
			continue;
		}

		if (cpu_count >= NR_CPUS)
			goto next;

		printk("cpu logical map 0x%llx\n", hwid);
		cpu_logical_map(cpu_count) = hwid;

		//early_map_cpu_to_node(cpu_count, of_node_to_nid(dn));
		early_write_pda(cpu_count, nodenumber, of_node_to_nid(dn));
next:
		cpu_count++;
	}
}

void __init smp_init_boot_cpu(void)
{
	cpu_logical_map(0) = mrs(MPIDR_EL1) & MPIDR_HWID_BITMASK;;
}

/*
 * Enumerate the possible CPU set from the device tree or ACPI and build the
 * cpu logical map array containing MPIDR values related to logical
 * cpus. Assumes that cpu_logical_map(0) has already been initialized.
 */
void __init smp_init_cpus(void)
{
#if 0

#endif

	int i;

	if (acpi_disabled)
		of_parse_and_init_cpus();
	else {
		panic("ACPI sucks.");
#if 0
		/*
		 * do a walk of MADT to determine how many CPUs
		 * we have including disabled CPUs, and get information
		 * we need for SMP init
		 */
		acpi_table_parse_madt(ACPI_MADT_TYPE_GENERIC_INTERRUPT,
				      acpi_parse_gic_cpu_interface, 0);
#endif 

	}
	if (cpu_count > nr_cpu_ids)
		printk("Number of cores (%d) exceeds configured maximum of %u - clipping\n",
			cpu_count, nr_cpu_ids);

	if (!bootcpu_valid) {
		printk("missing boot CPU MPIDR, not enabling secondaries\n");
		return;
	}

	/*
	 * We need to set the cpu_logical_map entries before enabling
	 * the cpus so that cpu processor description entries (DT cpu nodes
	 * and ACPI MADT entries) can be retrieved by matching the cpu hwid
	 * with entries in cpu_logical_map while initializing the cpus.
	 * If the cpu set-up fails, invalidate the cpu_logical_map entry.
	 */
	for (i = 1; i < nr_cpu_ids; i++) {
		if (cpu_logical_map(i) != INVALID_HWID) {
			if (smp_cpu_setup(i))
				cpu_logical_map(i) = INVALID_HWID;
		}
	}
}

void __init smp_prepare_boot_cpu(void)
{

	/* Assign the only CPU logical=physical ID 0 */
	cpu_set(0, cpu_present_map);
	physid_set(0, phys_cpu_present_map);
	cpu_info[0].logical_id   = 0;
	cpu_info[0].physical_id  = 0;
	cpu_info[0].arch.cpu_phys_id  = 0;




#if 0
	set_my_cpu_offset(per_cpu_offset(smp_processor_id()));
	/*
	 * Initialise the static keys early as they may be enabled by the
	 * cpufeature code.
	 */
	jump_label_init();
	cpuinfo_store_boot_cpu();
	save_boot_cpu_run_el();
	/*
	 * Run the errata work around checks on the boot CPU, once we have
	 * initialised the cpu feature infrastructure from
	 * cpuinfo_store_boot_cpu() above.
	 */
	update_cpu_errata_workarounds();
#endif
}



void __init smp_prepare_cpus(unsigned int max_cpus)
{
	int err;
	unsigned int cpu;
	unsigned int local_cpu;

	init_cpu_topology();

	local_cpu = smp_processor_id();
	store_cpu_topology(local_cpu);
	//	numa_store_cpu_info(local_cpu);

	/*
	 * If UP is mandated by "nosmp" (which implies "maxcpus=0"), don't set
	 * secondary CPUs present.
	 */
	if (max_cpus == 0)
		return;

	/*
	 * Initialise the present map (which describes the set of CPUs
	 * actually populated at the present time) and release the
	 * secondaries from the bootloader.
	 */
	for_each_possible_cpu(cpu) {

		per_cpu(cpu_number, cpu) = cpu;

		if (cpu == smp_processor_id())
			continue;

		if (!cpu_ops[cpu])
			continue;

		err = cpu_ops[cpu]->cpu_prepare(cpu);
		if (err)
			continue;

		set_cpu_present(cpu, true);
		//numa_store_cpu_info(cpu);
	}
}



