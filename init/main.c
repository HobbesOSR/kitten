#include <lwk/init.h>
#include <lwk/kernel.h>
#include <lwk/params.h>
#include <lwk/console.h>
#include <lwk/cpuinfo.h>
#include <lwk/percpu.h>
#include <lwk/smp.h>
DEFINE_PER_CPU(int, foo);

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
	printk(KERN_INFO  "Number of CPUs: %d\n", num_cpus());

	/*
 	 * Initialize per-CPU areas, one per CPU.
 	 * Variables defined with DEFINE_PER_CPU() end up in the per-CPU area.
 	 * This provides a mechanism for different CPUs to refer to their
 	 * private copy of the variable using the same name
 	 * (e.g., get_cpu_var(foo)).
 	 */
	setup_per_cpu_areas();

	/*
 	 * Initialize CPU exceptions/interrupts/traps.
 	 */
	//trap_init();
	interrupts_init();
	cpu_init();


	lapic_set_timer(1000000000);
	local_irq_enable();
	printk("Spinning forever...\n");
	while (1) {}
}

