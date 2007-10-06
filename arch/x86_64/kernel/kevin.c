#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/task.h>
#include <lwk/cpu.h>
#include <lwk/ptrace.h>
#include <lwk/string.h>
#include <arch/processor.h>
#include <arch/desc.h>
#include <arch/proto.h>

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
 * Sets up the MSRs needed to support the SYSCALL and SYSRET instructions.
 * Really, you should read the manual to understand these gems. In summary,
 * STAR and LSTAR specify the CS, SS, and RIP to install when the SYSCALL
 * instruction is issued. They also specify the CS and SS to install on SYSRET.
 *
 * On SYSCALL, the x86_64 CPU control unit uses STAR to load CS and SS and
 * LSTAR to load RIP.  The old RIP is saved in RCX.
 *
 * On SYSRET, the control unit uses STAR to restore CS and SS.
 * RIP is loaded from RCX.
 */
void __init
syscall_init(void)
{
	wrmsrl(MSR_STAR,  ((u64)__USER32_CS)<<48 | /* SYSRET  CS+SS */
	                  ((u64)__KERNEL_CS)<<32); /* SYSCALL CS+SS */
	wrmsrl(MSR_LSTAR, system_call);            /* SYSCALL RIP */
	wrmsrl(MSR_CSTAR, ignore_sysret);          /* RIP for compat. mode */

	/* RFLAGS to clear on SYSCALL */
	wrmsrl(MSR_SYSCALL_MASK, EF_TF|EF_DF|EF_IE|0x3000);
}

/**
 * Initializes and installs the calling CPU's Task State Segment (TSS).
 * The CPU's logical ID is passed in as input and must be correct.
 * The 'cpu' argument does *NOT* allow you to setup another CPU's TSS.
 */
void __init
tss_init(void)
{
	struct task *       me  = get_current_via_RSP();
	int                 cpu = me->cpu;
	struct tss_struct * tss = &per_cpu(init_tss, cpu);
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
 	 * Install the CPU's TSS.
 	 * Each CPU has its own TSS.
 	 */
	set_tss_desc(cpu, tss);
	load_TR_desc();
	load_LDT(&init_mm.context);
}

#if 0

void __init
cpu_init(void)
{
	int cpu;
	struct task *me;

	if (cpu_test_and_set(cpu, cpu_initialized_map))
		panic("CPU#%d already initialized!\n", cpu);
	printk(KERN_DEBUG "Initializing CPU#%d\n", cpu);

	/*
 	 * Get a reference to the currently executing task and the ID of the
 	 * CPU being initialized.  We can't use the normal 'current' mechanism
 	 * since it relies on the PDA being initialized, which it isn't for all
 	 * CPUs other than the boot CPU (id=0).  pda_init() is called below.
 	 */
	me  = get_current_via_RSP();	/* returns ptr to init task */
	cpu = me->cpu;			/* get CPU's logical ID */

	/*
 	 * Initialize this CPU's PDA (holds per-CPU data, each CPU has its own).
 	 * The boot CPU's PDA has already been initialized in head64.c.
 	 */
	if (cpu != 0)
		pda_init(cpu);

	/*
	 * Initialize the CPU's CR4 control register.  The bootstrap code has
	 * already partially setup this register.  We only touch the bits we
	 * care about, leaving the others untouched.
	 */
	clear_in_cr4(
		X86_CR4_VME | /* Disable Virtual-8086 support/cruft */
		X86_CR4_PVI | /* Disable support for VIF flag */
		X86_CR4_TSD | /* Allow RDTSC instruction at user-level */
		X86_CR4_DE    /* Disable debugging extensions */
	);

	/*
	 * Initialize the CPU's Global Descriptor Table.
	 * The boot CPU's GDT has already been setup.
	 * Each CPU has its own GDT.
	 */
	if (cpu != 0)
		memcpy(cpu_gdt(cpu), cpu_gdt_table, GDT_SIZE);
	cpu_gdt_descr[cpu].size = GDT_SIZE;
	asm volatile("lgdt %0" :: "m" (cpu_gdt_descr[cpu]));

	/*
 	 * Initialize the CPU's Interrupt Descriptor Table.
 	 * All CPU's share the same IDT.
 	 */
	asm volatile("lidt %0" :: "m" (idt_descr));

	/*
 	 * Zero the init-task's Thread-Local Storage (TLS) array.
 	 * A task's TLS array is loaded into the GDT when it is run.
 	 */
	memset(me->thread.tls_array, 0, GDT_ENTRY_TLS_ENTRIES * 8);

	/*
 	 * Initialize MSRs needed to support SYSCALL and SYSRET instuctions.
 	 */
	syscall_init();

	/*
 	 * pda_init() initialized MSR_GS_BASE already.  When the SWAPGS
 	 * instruction is issued, the x86_64 control unit atomically swaps
 	 * MSR_GS_BASE and MSR_KERNEL_GS_BASE.  So, when we call SWAPGS to
 	 * exit the kernel, the value in MSR_KERNEL_GS_BASE will be loaded.
 	 * User-space will see MSR_FS_BASE and MSR_GS_BASE both set to 0.
 	 */
	wrmsrl(MSR_FS_BASE, 0);
	wrmsrl(MSR_KERNEL_GS_BASE, 0);
	barrier();

	/*
 	 * Initialize the CPU's Task State Segment structure.
 	 */
	tss_init(void);

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

}
#endif
