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
#include <lwk/interrupt.h>
#include <lwk/sched.h>
#include <lwk/timer.h>
#include <lwk/kgdb.h>
#include <lwk/driver.h>
#include <lwk/kfs.h>
#include <lwk/random.h>
#include <lwk/linux_compat.h>


/**
 * Pristine copy of the LWK boot command line.
 *
 * Typically this is copied from the real mode data into kernel virtual
 * address space.
 */
char lwk_command_line[COMMAND_LINE_SIZE];


/**
 * Per-CPU flag used to limit memory accesses to only user memory.
 *
 * Certain system calls set this to true before calling downstream functions
 * to limit the downstream functions to only accessing user memory. It must
 * be reset to false before returning.
 */
DEFINE_PER_CPU(bool, umem_only);


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
 	 * e.g., the ones with param() and DRIVER_PARAM() specifiers.
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
	 * This detects memory, CPUs, architecture dependent irqs, etc.
	 */
	setup_arch();

	/*
	 * Setup the architecture independent interrupt handling.
	 */
	irq_init();

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
	sched_add_task(current);  /* now safe to call schedule() */

	/*
 	 * Initialize the task scheduling subsystem.
 	 */
	timer_subsys_init();

	/* Start the kernel filesystems */
	kfs_init();

	/*
	 * Initialize the random number generator.
	 */
	rand_init();

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

	/*
	 * Enable external interrupts.
	 */
	local_irq_enable();

	/*
	 * Bring up any network devices.
	 */
	netdev_init();

	/*
	 * And any modules that need to be started.
	 */
	driver_init_by_name( "module", "*" );

	/*
	 * Bring up any late init devices.
	 */
	driver_init_by_name( "late", "*" );

	/*
	 * Bring up the Linux compatibility layer, if enabled.
	 */
	linux_init();

	/*
	 * Start up user-space...
	 */
	printk(KERN_INFO "Loading initial user-level task (init_task)...\n");
	if ((status = create_init_task()) != 0)
		panic("Failed to create init_task (status=%d).", status);

	current->state = TASK_EXIT_ZOMBIE;
	schedule();  /* This should not return */
	BUG();
}
