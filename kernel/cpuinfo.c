#include <lwk/kernel.h>
#include <lwk/cpuinfo.h>

/**
 * Info structure for each CPU in the system.
 * Array is indexed by logical CPU ID.
 */
struct cpuinfo cpu_info[NR_CPUS];

/**
 * Map of all available CPUs.
 * This map represents logical CPU IDs.
 */
cpumask_t cpu_present_map;

/**
 * Map of all booted CPUs.
 * This map represents logical CPU IDs.
 * It will be a subset of cpu_present_map (usually identical after boot).
 */
cpumask_t cpu_online_map;

/**
 * Prints the input cpuinfo structure to the console.
 */
void print_cpuinfo(struct cpuinfo *c)
{
	printk("logical cpu id\t: %u\n", c->logical_id);
	print_arch_cpuinfo(c);
}

