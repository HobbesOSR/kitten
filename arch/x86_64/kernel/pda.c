#include <lwk/smp.h>
#include <lwk/task.h>
#include <lwk/init.h>
#include <arch/msr.h>
#include <arch/pda.h>

#if 0
/**
 * Memory for CPU0's IRQ_STACK.
 */
char boot_cpu_irqstack[IRQSTACKSIZE]
__attribute__((section(".bss.page_aligned")));

/**
 * This function initializes the calling CPU's PDA.
 *
 * When in kernel mode,
 * each CPU's GS.base is loaded with the address of the CPU's PDA.  This
 * allows data in the PDA to be accessed using segment relative accesses,
 * like:
 *
 *     movl $gs:pcurrent,%rdi	// move CPU's current task pointer to rdi
 *
 * This is similar to thread-local data for user-level programs.
 *
 * NOTE: The 'cpu' argument must be the logical ID of the calling CPU.
 *       The 'cpu' argument does not allow the calling CPU to initialize
 *       another CPU's PDA.
 */
void __init
pda_init(int cpu)
{ 
	struct x8664_pda *pda = cpu_pda(cpu);

	asm volatile("movl %0,%%fs ; movl %0,%%gs" :: "r" (0)); 

	mb();
	wrmsrl(MSR_GS_BASE, pda);
	mb();

	pda->cpunumber   = cpu;
	pda->irqcount    = -1;
	pda->kernelstack = (unsigned long)get_current_via_RSP()
	                       - PDA_STACKOFFSET
	                       + TASK_SIZE; 
//	pda->active_mm   = &init_mm;
	pda->mmu_state   = 0;

	if (cpu == 0) {
		/* others are initialized in smpboot.c */
		pda->pcurrent    = &init_task_union.task_info;
		pda->irqstackptr = boot_cpu_irqstack;
	} else {
		panic("in pda_init for CPU %d\n", cpu);
#if 0
		pda->irqstackptr = (char *) __get_free_pages(GFP_ATOMIC, IRQSTACK_ORDER);
		if (!pda->irqstackptr)
			panic("cannot allocate irqstack for cpu %d", cpu); 
#endif
	}

	pda->irqstackptr += IRQSTACKSIZE-64;
} 
#endif
