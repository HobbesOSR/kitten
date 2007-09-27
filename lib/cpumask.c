#include <lwk/kernel.h>
#include <lwk/bitops.h>
#include <lwk/cpumask.h>

int __first_cpu(const cpumask_t *srcp)
{
	return min_t(int, NR_CPUS, find_first_bit(srcp->bits, NR_CPUS));
}

int __next_cpu(int n, const cpumask_t *srcp)
{
	return min_t(int, NR_CPUS, find_next_bit(srcp->bits, NR_CPUS, n+1));
}

/*
 * Find the highest possible smp_cpu_id()
 *
 * Note: if we're prepared to assume that cpu_possible_map never changes
 * (reasonable) then this function should cache its return value.
 */
int highest_possible_cpu_id(void)
{
	unsigned int cpu;
	unsigned highest = 0;

	for_each_cpu_mask(cpu, cpu_present_map)
		highest = cpu;
	return highest;
}

int __any_online_cpu(const cpumask_t *mask)
{
	int cpu;

	for_each_cpu_mask(cpu, *mask) {
		if (cpu_online(cpu))
			break;
	}
	return cpu;
}
