#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/cpuinfo.h>
#include <lwk/bootmem.h>
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
 * This sets up the bootstrap memory allocator.  It is a simple
 * bitmap based allocator that tracks memory at a page grandularity.
 * Once the bootstrap process is complete, each unallocated page
 * is added to the real memory allocator's free pool.  Memory allocated
 * during bootstrap remains allocated forever, unless explicitly
 * freed before turning things over to the real memory allocator.
 */
static void __init
setup_bootmem_allocator(
	unsigned long	start_pfn,
	unsigned long	end_pfn
)
{
	unsigned long bootmap_size, bootmap;

	bootmap_size = bootmem_bootmap_pages(end_pfn)<<PAGE_SHIFT;
	bootmap = find_e820_area(0, end_pfn<<PAGE_SHIFT, bootmap_size);
	if (bootmap == -1L)
		panic("Cannot find bootmem map of size %ld\n",bootmap_size);
	bootmap_size = init_bootmem(bootmap >> PAGE_SHIFT, end_pfn);
	e820_bootmem_free(0, end_pfn << PAGE_SHIFT);
	reserve_bootmem(bootmap, bootmap_size);
}

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

	/*
 	 * It's now safe to destroy the virtual memory mappings from
 	 * [0, PAGE_OFFSET).  From here on out, the kernel accesses memory
 	 * using the identity map that we just set up, i.e., in the region
 	 * [PAGE_OFFSET, TOP_OF_MEMORY)
 	 */
	zap_low_mappings(0);
	
	/*
 	 * Initialize the bootstrap dynamic memory allocator.
 	 * alloc_bootmem() will work after this.
 	 */
	setup_bootmem_allocator(0, end_pfn);

	/*
	 * Mark reserved memory and io ports as, well... reserved.
	 */
	init_resources();

	printk("after init_resources()\n");
}

