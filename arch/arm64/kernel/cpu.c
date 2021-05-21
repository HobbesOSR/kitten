#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/task.h>
#include <lwk/cpu.h>
#include <lwk/ptrace.h>
#include <lwk/string.h>
#include <lwk/delay.h>
#include <arch/processor.h>
#include <arch/proto.h>
#include <arch/intc.h>
#include <arch/tsc.h>

#include <lwk/smp.h>
#include <lwk/init.h>
#include <lwk/bootmem.h>
#include <lwk/cpuinfo.h>
#include <lwk/params.h>
#include <arch/io.h>
#include <arch/mpspec.h>
#include <arch/proto.h>


/**
 * Bitmap of CPUs that have been initialized.
 */
static cpumask_t cpu_initialized_map;

/**
 * Initializes the calling CPU's Per-CPU Data Area (PDA).
 * When in kernel mode, each CPU's GS.base is loaded with the address of the
 * CPU's PDA. This allows data in the PDA to be accessed using segment relative
 * accesses, like:
 *
 *     movl $gs:pcurrent,%rdi	// move CPU's current task pointer to rdi
 *
 * This is similar to thread-local data for user-level programs.
 */
void __init
pda_init(unsigned int cpu, struct task_struct *task)
{
	struct ARM64_pda *pda = cpu_pda(cpu);

	mb();
	set_tpidr_el1((u64)pda);
	mb();


	pda->cpunumber     = cpu;
	pda->pcurrent      = task;
	pda->active_aspace = task->aspace;
	pda->kernelstack   = (vaddr_t)task + TASK_SIZE - PDA_STACKOFFSET;
	pda->mmu_state     = 0;
	pda->irqcount      = -1;
	mb();

}


/**
 * Installs the calling CPU's Local Descriptor Table (LDT).
 * All CPUs share the same IDT.
 */
static void __init
idt_init(void)
{
	/*
	 * The bootstrap CPU has already filled in the IDT table via the
	 * interrupts_init() call in setup.c. All we need to do is tell the CPU
	 * about it.
	 */
	//asm volatile("lidt %0" :: "m" (idt_descr));
}


/**
 * Initializes the calling CPU's debug registers.
 */
static void __init
dbg_init(void)
{
#if 0
	/*
 	 * Clear the CPU's debug registers.
 	 * DR[0-3] are Address-Breakpoint Registers
 	 * DR[4-5] are reserved and should not be used by software
 	 * DR6 is the Debug Status Register
 	 * DR7 is the Debug Control Register
 	 */
	set_debugreg(0UL, 0);
	set_debugreg(0UL, 1);
	set_debugreg(0UL, 2);
	set_debugreg(0UL, 3);
	set_debugreg(0UL, 6);
	set_debugreg(0UL, 7);
#endif
}

void __init
cpu_init(void)
{
	/*
 	 * Get a reference to the currently executing task and the ID of the
 	 * CPU being initialized.  We can't use the normal 'current' mechanism
 	 * since it relies on the PDA being initialized, which it isn't for all
 	 * CPUs other than the boot CPU (id=0). pda_init() is called below.
 	 */

	struct task_struct *me = get_current_via_RSP();
	unsigned int       cpu = me->cpu_id; /* logical ID */



	if (cpu_test_and_set(cpu, cpu_initialized_map))
		panic("CPU#%u already initialized!\n", cpu);

	early_printk(KERN_DEBUG "Initializing CPU#%u\n", cpu);

	pda_init(cpu, me);	/* per-cpu data area */

	identify_cpu();		/* determine cpu features via CPUID */
	//idt_init();		/* interrupt descriptor table */
	//dbg_init();		/* debug registers */
	//fpu_init();		/* floating point unit */
	intc_local_init();       /* Interrupt Controller */
	time_init();		/* detects CPU frequency, udelay(), etc. */
	barrier();		/* compiler memory barrier, avoids reordering */

}


int
phys_cpu_add(unsigned int phys_cpu_id, unsigned int apic_id)
{
	printk("Unhandled function %s\n",__FUNCTION__);
#if 0
	int logical_cpu;
	cpumask_t tmp_map;
	unsigned int timeout;
	unsigned char apic_ver;

	if (num_cpus >= NR_CPUS) {
		printk(KERN_ERR "NR_CPUS limits of %i reached.\n", NR_CPUS);
		return -1;
	}

	/* Count the new CPU */
	num_cpus++;

	/*
	 * Assign a logical CPU ID.
	 * Choose the lowest ID available
	 */
	 cpus_complement(tmp_map, cpu_present_map);
	 logical_cpu = first_cpu(tmp_map);

	/* JRL:
	 * This is most likely a bad idea, but I don't know a better thing to do
	 * We don't have the MPTable information, which is where this is normally stored
	 */
	 apic_ver = apic_version[0];

	 /*
	   if (apic_ver == 0x0) {
		printk(KERN_ERR "BIOS bug, APIC version is 0 for PhysCPU 0! "
		                "fixing up to 0x10. (tell your hw vendor)\n");
		apic_ver = 0x10;
	 }
	 */
	apic_version[phys_cpu_id] = apic_ver;


	/* Add the CPU to the map of physical CPU IDs present. */
	physid_set(phys_cpu_id, phys_cpu_present_map);

	/* Add the CPU to the map of logical CPU IDs present. */
	cpu_set(logical_cpu, cpu_present_map);

	/* We now have to at minimum update the KERNEL ASPACE to allow this CPU */
	/* NOTE: No other existing ASPACEs will be updated, so they will not be
	   accessible on this CPU unless they are explicitly updated.
	   BUT: the feature to do that does not exist yet!!!!
	*/
	__aspace_update_cpumask(KERNEL_ASPACE_ID, &cpu_present_map);

	/* Store ID information. */
	cpu_info[logical_cpu].logical_id   = logical_cpu;
	cpu_info[logical_cpu].physical_id  = phys_cpu_id;
	cpu_info[logical_cpu].arch.apic_id = apic_id;

	printk(KERN_DEBUG
	  "Booting Physical CPU #%d -> Logical CPU #%d APIC #%d (APIC version %d)\n",
	       phys_cpu_id,
	       logical_cpu,
	       apic_id,
	       apic_version[phys_cpu_id]);

	setup_per_cpu_area(logical_cpu);

	arch_boot_cpu(logical_cpu);

	/* Wait for ACK that CPU has booted (5 seconds max). */
	for (timeout = 0; timeout < 50000; timeout++) {
	    if (cpu_isset(logical_cpu, cpu_online_map))
		break;
	    udelay(100);
	}

	if (!cpu_isset(logical_cpu, cpu_online_map)) {
	    panic("Failed to boot CPU %d.\n", logical_cpu);
	} else {
	    printk("Logical CPU %d is now online\n", logical_cpu);
	}

	return logical_cpu;
#endif
	return 0;
}

/*
 * Hot remove a target cpu
 */
int
phys_cpu_remove(unsigned int phys_cpu_id,
		unsigned int apic_id)
{
	printk("Unhandled function %s\n",__FUNCTION__);
#if 0
        unsigned int logical_id;
        unsigned int target_cpu = -1;
        unsigned int timeout;
        cpumask_t    target_cpu_mask;

        for_each_cpu_mask(logical_id, cpu_present_map) {
		if ((cpu_info[logical_id].physical_id  == phys_cpu_id) &&
		    (cpu_info[logical_id].arch.apic_id == apic_id)) {

			target_cpu = logical_id;
			break;
		}
        }

        if (target_cpu == -1) {
                panic("Failed to found target cpu when offlining:"
		      "phys_id %u, apic_id %u", phys_cpu_id, apic_id);
                return -1;
        }

        if ((cpu_info[target_cpu].physical_id  != phys_cpu_id) ||
	    (cpu_info[target_cpu].arch.apic_id != apic_id)) {
                panic("Inconsistent CPU found when offlining\n"
		      "  cpu_info: target_cpu %d (phys %u, apic %u)\n"
		      "  requested: phys %u, apic %u\n",
		      target_cpu,
		      cpu_info[target_cpu].physical_id,
		      cpu_info[target_cpu].arch.apic_id,
		      phys_cpu_id,
		      apic_id);

                return -1;
        }

        cpus_clear(target_cpu_mask);
        cpu_set(target_cpu, target_cpu_mask);

	/* Signal remote CPU to offline itself */
        xcall_function(target_cpu_mask, sched_cpu_remove, NULL, 0);

        /* Wait for ACK that CPU has offlined (5 seconds max). */
        for (timeout = 0; timeout < 50000; timeout++) {
		if (!cpu_isset(target_cpu, cpu_online_map)) {
			break;
		}

		udelay(100);
        }

        if (cpu_isset(target_cpu, cpu_online_map)) {
		panic("Failed to offline CPU %d.\n", target_cpu);
        } else {
		printk("Logical CPU %d (phys_id %d, apic_id %d) is now offline\n",
		       target_cpu,
		       cpu_info[target_cpu].physical_id,
		       cpu_info[target_cpu].arch.apic_id);

		apic_version[phys_cpu_id] = 0;
		cpu_clear(target_cpu, cpu_present_map);
		cpu_clear(target_cpu, cpu_initialized_map);
		physid_clear(phys_cpu_id, phys_cpu_present_map);
		__aspace_update_cpumask(KERNEL_ASPACE_ID, &cpu_present_map);

		kmem_free_pages((void *)cpu_gdt_descr[target_cpu].address, 0);
		free_per_cpu_area(target_cpu);
        }
#endif
        return 0;
}



void
arch_shutdown_cpu(void)
{
	/* The LAPIC can end up in a weird state if we go into cli/hlt without
	 * acking the EOI first. Of course, this is only the case if we are in
	 * interrupt
	 */

#if 0
	if (in_interrupt())
		lapic_ack_interrupt();
#endif

	local_irq_disable();

	while (1) {
	    halt();
	    cpu_relax();
	}
}
