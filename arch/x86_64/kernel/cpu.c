#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/task.h>
#include <lwk/cpu.h>
#include <lwk/ptrace.h>
#include <lwk/string.h>
#include <lwk/delay.h>
#include <arch/processor.h>
#include <arch/desc.h>
#include <arch/proto.h>
#include <arch/i387.h>
#include <arch/apic.h>
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
	struct x8664_pda *pda = cpu_pda(cpu);

	/* 
 	 * Point FS and GS at the NULL segment descriptor (entry 0) in the GDT.
 	 * x86_64 does away with a lot of segmentation cruftiness... there's no
 	 * need to set up specific GDT entries for FS or GS.
 	 */
	asm volatile("movl %0,%%fs ; movl %0,%%gs" :: "r" (0));

	/*
 	 * Load the address of this CPU's PDA into this CPU's GS_BASE model
 	 * specific register. Upon entry to the kernel, the SWAPGS instruction
 	 * is used to load the value from MSR_GS_BASE into the GS segment
 	 * register's base address (GS.base).  The user-level GS.base value
 	 * is stored in MSR_GS_BASE.  When the kernel is exited, SWAPGS is
 	 * called again.
 	 */
	mb();
	wrmsrl(MSR_GS_BASE, pda);
	mb();

	pda->cpunumber   = cpu;
	pda->pcurrent    = task;
	pda->active_mm   = task->mm;
	pda->kernelstack = (unsigned long)task - PDA_STACKOFFSET + TASK_SIZE;
	pda->mmu_state   = 0;
}

/**
 * Initializes the calling CPU's Control Register 4 (CR4).
 * The bootstrap assembly code has already partially setup this register.
 * We only touch the bits we care about, leaving the others untouched.
 */
static void __init
cr4_init(void)
{
	clear_in_cr4(
		X86_CR4_VME | /* Disable Virtual-8086 support/cruft */
		X86_CR4_PVI | /* Disable support for VIF flag */
		X86_CR4_TSD | /* Allow RDTSC instruction at user-level */
		X86_CR4_DE    /* Disable debugging extensions */
	);
}

/**
 * Initializes and installs the calling CPU's Global Descriptor Table (GDT).
 * Each CPU has its own GDT.
 */
static void __init
gdt_init(void)
{
	unsigned int cpu = cpu_id();

	/* The bootstrap CPU's GDT has already been setup */
	if (cpu != 0)
		memcpy(cpu_gdt(cpu), cpu_gdt_table, GDT_SIZE);
	cpu_gdt_descr[cpu].size = GDT_SIZE;

	/* Install the CPU's GDT */
	asm volatile("lgdt %0" :: "m" (cpu_gdt_descr[cpu]));

	/*
	 * Install the CPU's LDT... Local Descriptor Table.
	 * We have no need for a LDT, so we point it at the NULL descriptor.
	 */
	asm volatile("lldt %w0":: "r" (0));
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
	asm volatile("lidt %0" :: "m" (idt_descr));
}

/**
 * Initializes and installs the calling CPU's Task State Segment (TSS).
 */
static void __init
tss_init(void)
{
	unsigned int       cpu  = cpu_id();
	struct tss_struct  *tss = &per_cpu(init_tss, cpu);
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
	asm volatile("ltr %w0":: "r" (GDT_ENTRY_TSS*8));
}

/**
 * Initializes various Model Specific Registers (MSRs) of the calling CPU.
 */
static void __init
msr_init(void)
{
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
}

/**
 * Initializes the calling CPU's debug registers.
 */
static void __init
dbg_init(void)
{
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

extern unsigned int pit_calibrate_tsc(void);

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
	unsigned int       cpu = me->cpu; /* logical ID */

	if (cpu_test_and_set(cpu, cpu_initialized_map))
		panic("CPU#%u already initialized!\n", cpu);
	printk(KERN_DEBUG "Initializing CPU#%u\n", cpu);

	pda_init(cpu, me);	/* per-cpu data area */
	cr4_init();		/* control register 4 */
	gdt_init();		/* global descriptor table */
	idt_init();		/* interrupt descriptor table */
	tss_init();		/* task state segment */
	msr_init();		/* misc. model specific registers */
	dbg_init();		/* debug registers */
	fpu_init();		/* floating point unit */
	lapic_init();		/* local advanced prog. interrupt controller */
	time_init();		/* detects CPU frequency, udelay(), etc. */
	barrier();		/* compiler memory barrier, avoids reordering */
}

void
cpu_idle(void)
{
	printk(KERN_DEBUG "Entering Idle Loop\n");
	while (1) {
		//while (!need_resched) {}
	}
}


