#include <lwk/init.h>
#include <lwk/kernel.h>
#include <lwk/params.h>
#include <lwk/console.h>
#include <lwk/cpuinfo.h>
#include <lwk/percpu.h>
#include <lwk/smp.h>
#include <lwk/cpuinfo.h>
#include <lwk/delay.h>

/**
 * Pristine copy of the LWK boot command line.
 */
char lwk_command_line[COMMAND_LINE_SIZE];

/**
 * This is the architecture-independent kernel entry point. Before it is
 * called, architecture-specific code has done the bare minimum initialization
 * necessary. This function initializes the kernel and its various subsystems.
 * It calls back to architecture-specific code at several well defined points,
 * which all architectures must implement (e.g., setup_arch()).
 */
void
start_kernel()
{
	unsigned int cpu;
	unsigned int timeout;

	/*
 	 * Parse the kernel boot command line.
 	 * This is where boot-time configurable variables get set,
 	 * e.g., the ones with param() and driver_param() specifiers.
 	 */
	parse_params(lwk_command_line);

	/*
 	 * Initialize the console subsystem.
 	 * printk()'s will be visible after this.
 	 */
	console_init();

	/*
	 * Hello, Dave.
	 */
	printk(lwk_banner);
	printk(KERN_DEBUG "%s\n", lwk_command_line);

	/*
	 * Do architecture specific initialization.
	 * This detects memory, CPUs, etc.
	 */
	setup_arch();
	printk(KERN_INFO "Number of CPUs detected: %d\n", num_cpus());

	/*
	 * Boot all CPUs in the system, one at a time.
	 */
	for_each_cpu_mask(cpu, cpu_present_map) {
		/* The bootstrap CPU (that's us) is already booted. */
		if (cpu == 0)
			continue;

		printk(KERN_DEBUG "Booting CPU %u.\n", cpu);
		arch_boot_cpu(cpu);

		/* Wait for ACK that CPU has booted (5 seconds max). */
		for (timeout = 0; timeout < 50000; timeout++) {
			if (cpu_isset(cpu, cpu_online_map))
				break;
			udelay(100);
		}

		if (!cpu_isset(cpu, cpu_online_map))
			panic("Failed to boot CPU %d.\n", cpu);
	}

	printk(KERN_DEBUG "Spinning forever...\n");
	cpu_idle();
}

