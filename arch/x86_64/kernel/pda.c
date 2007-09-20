#include <lwk/smp.h>
#include <lwk/task.h>
#include <lwk/init.h>
#include <arch/msr.h>
#include <arch/pda.h>
#include <arch/proto.h> // TODO: remove this, needed for boot_cpu_stack

/**
 * Array of pointers to each CPU's per-processor data area.
 * The array is indexed by CPU ID.
 */
struct x8664_pda *_cpu_pda[NR_CPUS] __read_mostly;

/**
 * Array of per-processor data area structures, one per CPU.
 * The array is indexed by CPU ID.
 */
struct x8664_pda boot_cpu_pda[NR_CPUS] __cacheline_aligned;

/**
 * Initializes the PDA of the specified CPU ID.
 */
void __init pda_init(int cpu)
{ 
	struct x8664_pda *pda = cpu_pda(cpu);

	/* Setup up data that may be needed in __get_free_pages early */
	asm volatile("movl %0,%%fs ; movl %0,%%gs" :: "r" (0)); 
	wrmsrl(MSR_GS_BASE, pda);

	pda->cpunumber   = cpu;
	pda->irqcount    = -1;
	pda->kernelstack = (unsigned long)stack_to_task() - PDA_STACKOFFSET + TASK_SIZE; 
//	pda->active_mm   = &init_mm;
	pda->mmu_state   = 0;

	if (cpu == 0) {
		/* others are initialized in smpboot.c */
		pda->pcurrent    = &init_task_union.task_info;
		pda->irqstackptr = boot_cpu_stack; 
	} else {
#if 0
		pda->irqstackptr = (char *) __get_free_pages(GFP_ATOMIC, IRQSTACK_ORDER);
		if (!pda->irqstackptr)
			panic("cannot allocate irqstack for cpu %d", cpu); 
#endif
	}

	pda->irqstackptr += IRQSTACKSIZE-64;
} 

