#include <lwk/kernel.h>
#include <lwk/cpuinfo.h>

/**
 * Information about each CPU in the system.
 * Array is indexed by logical CPU ID.
 */
struct cpuinfo cpu_info[NR_CPUS];

/**
 * Prints the input cpuinfo structure to the console.
 */
void print_cpuinfo(struct cpuinfo *c)
{
	printk("logical cpu id\t: %u\n", c->logical_id);
	print_arch_cpuinfo(c);
}
