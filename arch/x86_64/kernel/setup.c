#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/cpuinfo.h>
#include <arch/e820.h>
#include <arch/page.h>
#include <arch/proto.h>

/**
 * Bitmap of of PTE/PMD entry flags that are supported.
 * This is AND'ed with a PTE/PMD entry before it is installed.
 */
unsigned long __supported_pte_mask __read_mostly = ~0UL;

/**
 * Bitmap of features enabled in the CR4 register.
 */
unsigned long mmu_cr4_features;

/**
 * Architecture specific initialization.
 * This is called from start_kernel() in init/main.c.
 */
void __init
setup_arch(void)
{
	/*
	 * Figure out which memory regions are usable and which are reserved.
	 * This builds the "e820" map of memory from info provided by the
	 * BIOS.
	 */
	setup_memory_region();

	/*
 	 * Get the bare minimum info about the bootstrap CPU... the
 	 * one we're executing on right now.  Latter on, the full
 	 * boot_cpu_info and cpu_info[boot_cpu_id] structures will be
 	 * filled in completely.
 	 */
	boot_cpu_info.logical_id = 0;
	early_identify_cpu(&boot_cpu_info);

	/*
	 * Find the Extended BIOS Data Area.
	 * (Not sure why exactly we need this, probably don't.)
	 */ 
	discover_ebda();

	/*
	 * Initialize the kernel page tables.
	 * The kernel page tables map an "identity" map of all physical memory
	 * starting at virtual address PAGE_OFFSET.  When the kernel executes,
	 * it runs inside of the identity map... memory below PAGE_OFFSET is
	 * from whatever task was running when the kernel got invoked.
	 */
	init_kernel_pgtables(0, (end_pfn_map << PAGE_SHIFT));
}
