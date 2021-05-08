#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/task.h>
#include <lwk/cpu.h>
#include <lwk/ptrace.h>
#include <lwk/string.h>
#include <lwk/delay.h>
#include <arch/processor.h>
//#include <arch/desc.h>
#include <arch/proto.h>
//#include <arch/i387.h>
#include <arch/tsc.h>

/**
 * Bitmap of CPUs that have been initialized.
 */
static cpumask_t cpu_initialized_map;

/**
 * Memory for STACKFAULT stacks, one for each CPU.
 */
char stackfault_stack[NR_CPUS][PAGE_SIZE]
__attribute__((section(".bss.page_aligned")));

/**
 * Memory for DOUBLEFAULT stacks, one for each CPU.
 */
char doublefault_stack[NR_CPUS][PAGE_SIZE]
__attribute__((section(".bss.page_aligned")));

/**
 * Memory for NMI stacks, one for each CPU.
 */
char nmi_stack[NR_CPUS][PAGE_SIZE]
__attribute__((section(".bss.page_aligned")));

/**
 * Memory for DEBUG stacks, one for each CPU.
 */
char debug_stack[NR_CPUS][PAGE_SIZE]
__attribute__((section(".bss.page_aligned")));

/**
 * Memory for MCE stacks, one for each CPU.
 */
char mce_stack[NR_CPUS][PAGE_SIZE]
__attribute__((section(".bss.page_aligned")));


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
 * Initializes and installs the calling CPU's Global Descriptor Table (GDT).
 * Each CPU has its own GDT.
 */
static void __init
gdt_init(void)
{
#if 0
	unsigned int cpu = this_cpu;

	/* The bootstrap CPU's GDT has already been setup */
	if (cpu != 0)
		memcpy(cpu_gdt(cpu), cpu_gdt_table, GDT_SIZE);
	cpu_gdt_descr[cpu].size = GDT_SIZE;

	/* Install the CPU's GDT */
	//asm volatile("lgdt %0" :: "m" (cpu_gdt_descr[cpu]));

	/*
	 * Install the CPU's LDT... Local Descriptor Table.
	 * We have no need for a LDT, so we point it at the NULL descriptor.
	 */
	//asm volatile("lldt %w0":: "r" (0));
#endif
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
 * Initializes and installs the calling CPU's Task State Segment (TSS).
 */
static void __init
tss_init(void)
{
#if 0
	unsigned int       cpu  = this_cpu;
	struct tss_struct  *tss = &per_cpu(tss, cpu);
	int i;

	/*
	 * Initialize the CPU's Interrupt Stack Table.
	 * Certain exceptions and interrupts are handled with their own,
	 * known good stack. The IST holds the address of these stacks.
	 */
	tss->ist[STACKFAULT_STACK-1]  = (unsigned long)&stackfault_stack[cpu][0];
	tss->ist[DOUBLEFAULT_STACK-1] = (unsigned long)&doublefault_stack[cpu][0];
	tss->ist[NMI_STACK-1]         = (unsigned long)&nmi_stack[cpu][0];
	tss->ist[DEBUG_STACK-1]       = (unsigned long)&debug_stack[cpu][0];
	tss->ist[MCE_STACK-1]         = (unsigned long)&mce_stack[cpu][0];

	/*
	 * Initialize the CPU's I/O permission bitmap.
	 * The <= is required because the CPU will access up to 8 bits beyond
	 * the end of the IO permission bitmap.
	 */
	tss->io_bitmap_base = offsetof(struct tss_struct, io_bitmap);
	for (i = 0; i <= IO_BITMAP_LONGS; i++) 
		tss->io_bitmap[i] = ~0UL;

	/*
 	 * Install the CPU's TSS and load the CPU's Task Register (TR).
 	 * Each CPU has its own TSS.
 	 */
	set_tss_desc(cpu, tss);
	//asm volatile("ltr %w0":: "r" (GDT_ENTRY_TSS*8));
#endif
}

/**
 * Initializes various Model Specific Registers (MSRs) of the calling CPU.
 */
static void __init
msr_init(void)
{
#if 0
	/*
	 * Setup the MSRs needed to support the SYSCALL and SYSRET
	 * instructions. Really, you should read the manual to understand these
	 * gems. In summary, STAR and LSTAR specify the CS, SS, and RIP to
	 * install when the SYSCALL instruction is issued. They also specify the
	 * CS and SS to install on SYSRET.
	 *
	 * On SYSCALL, the x86_64 CPU control unit uses STAR to load CS and SS and
	 * LSTAR to load RIP. The old RIP is saved in RCX.
	 *
	 * On SYSRET, the control unit uses STAR to restore CS and SS.
	 * RIP is loaded from RCX.
	 *
	 * SYSCALL_MASK specifies the RFLAGS to clear on SYSCALL.
	 */
	wrmsrl(MSR_STAR,  ((u64)__USER32_CS)<<48 | /* SYSRET  CS+SS */
	                  ((u64)__KERNEL_CS)<<32); /* SYSCALL CS+SS */
	wrmsrl(MSR_LSTAR, asm_syscall);            /* SYSCALL RIP */
	wrmsrl(MSR_CSTAR, asm_syscall_ignore);     /* RIP for compat. mode */
	wrmsrl(MSR_SYSCALL_MASK, EF_TF|EF_DF|EF_IE|0x3000);

	/*
	 * Setup MSRs needed to support the PDA.
 	 * pda_init() initialized MSR_GS_BASE already. When the SWAPGS
 	 * instruction is issued, the x86_64 control unit atomically swaps
 	 * MSR_GS_BASE and MSR_KERNEL_GS_BASE. So, when we call SWAPGS to
 	 * exit the kernel, the value in MSR_KERNEL_GS_BASE will be loaded.
 	 * User-space will see MSR_FS_BASE and MSR_GS_BASE both set to 0.
 	 */
	wrmsrl(MSR_FS_BASE, 0);
	wrmsrl(MSR_KERNEL_GS_BASE, 0);
#endif
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

	printk(KERN_DEBUG "Initializing CPU#%u\n", cpu);

	pda_init(cpu, me);	/* per-cpu data area */

	identify_cpu();		/* determine cpu features via CPUID */
	//cr4_init();		/* control register 4 */
	//gdt_init();		/* global descriptor table */
	//idt_init();		/* interrupt descriptor table */
	//tss_init();		/* task state segment */
	//msr_init();		/* misc. model specific registers */
	//dbg_init();		/* debug registers */
	//fpu_init();		/* floating point unit */
	//lapic_init();		/* local advanced prog. interrupt controller */
	//time_init();		/* detects CPU frequency, udelay(), etc. */
	//barrier();		/* compiler memory barrier, avoids reordering */

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
