#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/cpuinfo.h>
#include <lwk/bootmem.h>
#include <arch/bootsetup.h>
#include <arch/e820.h>
#include <arch/page.h>
#include <arch/sections.h>
#include <arch/smp.h>
#include <arch/proto.h>
#include <arch/mpspec.h>

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
 * Start and end addresses of the initrd image.
 */
unsigned long initrd_start;
unsigned long initrd_end;

/**
 * Base address and size of the Extended BIOS Data Area.
 */
unsigned long __initdata ebda_addr;
unsigned long __initdata ebda_size;
#define EBDA_ADDR_POINTER 0x40E

/**
 * Finds the address and length of the Extended BIOS Data Area.
 */
static void __init
discover_ebda(void)
{
	/*
	 * There is a real-mode segmented pointer pointing to the 
	 * 4K EBDA area at 0x40E
	 */
	ebda_addr = *(unsigned short *)EBDA_ADDR_POINTER;
	ebda_addr <<= 4;

	ebda_size = *(unsigned short *)(unsigned long)ebda_addr;

	/* Round EBDA up to pages */
	if (ebda_size == 0)
		ebda_size = 1;
	ebda_size <<= 10;
	ebda_size = round_up(ebda_size + (ebda_addr & ~PAGE_MASK), PAGE_SIZE);
	if (ebda_size > 64*1024)
		ebda_size = 64*1024;
}

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
 * Mark in-use memory regions as reserved.
 * This prevents the bootmem allocator from allocating them.
 */
static void __init
reserve_memory(void)
{
	/* Reserve the kernel page table memory */
	reserve_bootmem(table_start << PAGE_SHIFT,
	                (table_end - table_start) << PAGE_SHIFT);

	/* Reserve kernel memory */
	reserve_bootmem(__pa_symbol(&_text),
	                __pa_symbol(&_end) - __pa_symbol(&_text));

	/* Reserve physical page 0... it's a often a special BIOS page */
	reserve_bootmem(0, PAGE_SIZE);

	/* Reserve the Extended BIOS Data Area memory */
	if (ebda_addr)
		reserve_bootmem(ebda_addr, ebda_size);

	/* Reserve SMP trampoline */
	reserve_bootmem(SMP_TRAMPOLINE_BASE, PAGE_SIZE);

	/* Find and reserve boot-time SMP configuration */
	find_mp_config();

	/* Reserve memory used by the initrd image */
	if (LOADER_TYPE && INITRD_START) {
		if (INITRD_START + INITRD_SIZE <= (end_pfn << PAGE_SHIFT)) {
			reserve_bootmem(INITRD_START, INITRD_SIZE);
			initrd_start =
				INITRD_START ? INITRD_START + PAGE_OFFSET : 0;
			initrd_end = initrd_start+INITRD_SIZE;
		} else {
			printk(KERN_ERR "initrd extends beyond end of memory "
			    "(0x%08lx > 0x%08lx)\ndisabling initrd\n",
			    (unsigned long)(INITRD_START + INITRD_SIZE),
			    (unsigned long)(end_pfn << PAGE_SHIFT));
			initrd_start = 0;
		}
	}
}

/**
 * Architecture specific initialization.
 * This is called from start_kernel() in init/main.c.
 *
 * NOTE: Ordering is usually important.  Do not move things
 *       around unless you know what you are doing.
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
	boot_cpu_data.logical_id = 0;
	early_identify_cpu(&boot_cpu_data);

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
 	 * using the identity map that we just set up, i.e., in the (virtual)
 	 * region [PAGE_OFFSET, TOP_OF_MEMORY).
 	 */
	zap_low_mappings(0);
	
	/*
 	 * Initialize the bootstrap dynamic memory allocator.
 	 * alloc_bootmem() will work after this.
 	 */
	setup_bootmem_allocator(0, end_pfn);
	reserve_memory();

	/*
	 * Get the multiprocessor configuration...
	 * number of CPUs, PCI bus info, APIC info, etc.
	 */
	get_mp_config();

	/*
	 * Initialize resources.  Resources reserve sections of normal memory
	 * (iomem) and I/O ports (ioport) for devices and other system
	 * resources.  For each resource type, there is a tree which tracks
	 * which regions are in use.  This eliminates the possiblity of
	 * conflicts... e.g., two devices trying to use the same iomem region.
	 */
	init_resources();
}

