#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/spinlock.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/time.h>
#include <arch/io.h>





void __init
time_init(void)
{
	uint32_t cpu_khz = (u32)mrs(CNTFRQ_EL0) / 1000;
	/*
	 * Cache the CPU frequency
	 */

	struct arch_cpuinfo * const arch_info = &cpu_info[this_cpu].arch;
	arch_info->cur_cpu_khz = cpu_khz;
	arch_info->max_cpu_khz = cpu_khz;
	arch_info->min_cpu_khz = cpu_khz;
	arch_info->min_cpu_khz = cpu_khz;
	arch_info->tsc_khz     = cpu_khz;

	init_cycles2ns(cpu_khz);


	printk(KERN_DEBUG "CPU %u: %u.%03u MHz\n",
		this_cpu,
		cpu_khz / 1000, cpu_khz % 1000
	);
}

