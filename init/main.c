/** \file
 * Architecture-independent kernel entry.
 */
#include <lwk/extable.h>
#include <lwk/init.h>
#include <lwk/kernel.h>
#include <lwk/params.h>
#include <lwk/console.h>
#include <lwk/netdev.h>
#include <lwk/cpuinfo.h>
#include <lwk/percpu.h>
#include <lwk/smp.h>
#include <lwk/cpuinfo.h>
#include <lwk/delay.h>
#include <lwk/bootmem.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <lwk/sched.h>
#include <lwk/timer.h>
#include <lwk/kgdb.h>
#include <lwk/driver.h>
#include <lwk/kfs.h>
#ifdef CONFIG_PALACIOS
#include <arch/palacios.h>
#endif

/**
 * Pristine copy of the LWK boot command line.
 *
 * Typically this is copied from the real mode data into kernel virtual
 * address space.
 */
char lwk_command_line[COMMAND_LINE_SIZE];


/**
 * This is the architecture-independent kernel entry point. Before it is
 * called, architecture-specific code has done the bare minimum initialization
 * necessary. This function initializes the kernel and its various subsystems.
 * It calls back to architecture-specific code at several well defined points,
 * which all architectures must implement (e.g., setup_arch()).
 *
 * \callgraph
 */
void
start_kernel()
{
	unsigned int cpu;
	unsigned int timeout;
	int status;

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
	printk("%s", lwk_banner);
	printk(KERN_DEBUG "%s\n", lwk_command_line);
	sort_exception_table();
	/*
	 * Do architecture specific initialization.
	 * This detects memory, CPUs, etc.
	 */
	setup_arch();

	/*
	 * Initialize the kernel memory subsystem. Up until now, the simple
	 * boot-time memory allocator (bootmem) has been used for all dynamic
	 * memory allocation. Here, the bootmem allocator is destroyed and all
	 * of the free pages it was managing are added to the kernel memory
	 * pool (kmem) or the user memory pool (umem).
	 *
	 * After this point, any use of the bootmem allocator will cause a
	 * kernel panic. The normal kernel memory subsystem API should be used
	 * instead (e.g., kmem_alloc() and kmem_free()).
	 */
	mem_subsys_init();

	/*
 	 * Initialize the address space management subsystem.
 	 */
	aspace_subsys_init();

	/*
 	 * Initialize the task management subsystem.
 	 */
	task_subsys_init();

	/*
 	 * Initialize the task scheduling subsystem.
 	 */
	sched_subsys_init();

	/*
 	 * Initialize the task scheduling subsystem.
 	 */
	timer_subsys_init();

	/* Start the kernel filesystems */
	kfs_init();

	/*
	 * Bring up any network devices.
	 */
	netdev_init();

	/*
	 * Bring up any late init devices.
	 */
	driver_init_by_name( "late", "*" );

	/*
	 * Boot all of the other CPUs in the system, one at a time.
	 */
	printk(KERN_INFO "Number of CPUs detected: %d\n", num_cpus());
	for_each_cpu_mask(cpu, cpu_present_map) {
		/* The bootstrap CPU (that's us) is already booted. */
		if (cpu == 0) {
			cpu_set(cpu, cpu_online_map);
			continue;
		}

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

#ifdef CONFIG_KGDB
        kgdb_initial_breakpoint();
#endif

#ifdef CONFIG_PALACIOS
	/*
 	 * Start up a guest operating system...
 	 */
	printk(KERN_INFO "Loading initial guest operating system...\n");
	status = palacios_run_guest();  /* This should not return */
	panic("palacios_run_guest() returned (status=%d).", status);
#else
	/*
	 * Start up user-space...
	 */
	printk(KERN_INFO "Loading initial user-level task (init_task)...\n");
	if ((status = create_init_task()) != 0)
		panic("Failed to create init_task (status=%d).", status);

	schedule();  /* This should not return */
	BUG();
#endif
}
