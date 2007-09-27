#include <lwk/kernel.h>
#include <arch/page.h>
#include <arch/pgtable.h>
#include <arch/e820.h> 
#include <arch/tlbflush.h>

/**
 * Start and end page frame numbers of the kernel page tables.
 */
unsigned long __initdata table_start, table_end;  /* page frame numbers */

/**
 * Memory for temp_boot_pmds[] is reserved in arch/x86_64/kernel/head.S
 */
extern pmd_t temp_boot_pmds[]; 

/**
 * These are used to map in pages of physical memory into the kernel
 * on-the-fly before the identity map is fully initialized.  * Yes, this is a bit convoluted.
 */
static  struct temp_map { 
	pmd_t *pmd;
	void  *address; 
	int    allocated; 
} temp_mappings[] __initdata = { 
	{ &temp_boot_pmds[0], (void *)(40UL * 1024 * 1024), 0 },
	{ &temp_boot_pmds[1], (void *)(42UL * 1024 * 1024), 0 }, 
	{ NULL, NULL, 0 }
}; 

/**
 * Finds enough space for the kernel page tables.
 */
static void __init
find_early_table_space(unsigned long end)
{
	unsigned long puds;	/* # of pud page tables needed */
	unsigned long pmds;	/* # of pmd page tables needed */
	unsigned long tables;	/* page table memory needed, in bytes */
	unsigned long start;	/* start address for kernel page tables */

	/*
 	 * The kernel page tables map memory using 2 MB pages.
	 * This means only puds and pmds are needed.
	 */
	puds = (end + PUD_SIZE - 1) >> PUD_SHIFT;
	pmds = (end + PMD_SIZE - 1) >> PMD_SHIFT;
	tables = round_up(puds * sizeof(pud_t), PAGE_SIZE) +
		 round_up(pmds * sizeof(pmd_t), PAGE_SIZE);

	/*
 	 * Consult the memory map to find a region of suitable size.
 	 */
 	start = 0x8000;
 	table_start = find_e820_area(start, end, tables);
	if (table_start == -1UL)
		panic("Cannot find space for the kernel page tables");

	/*
 	 * Store table_start and table_end as page frame numbers.
	 * table_end starts out as the same as table_start.
	 * It will be incremented for each page table allocated.
	 */
	table_start >>= PAGE_SHIFT;
	table_end = table_start;
}

/**
 * Allocates a page of physical memory and maps it into the kernel's address
 * space using a temporary mapping.
 *
 * Returns:
 *     The kernel virtual address of the allocated page (temporary mapping)
 *
 * Output arguments:
 *     *index = cookie needed to destroy the temporary mapping
 *                  pass to unmap_low_pages()
 *     *phys  = the physical address of the new page
 */
static void * __init
alloc_low_page(int *index, unsigned long *phys)
{ 
	struct temp_map *ti;
	int i; 
	unsigned long pfn = table_end++, paddr; 
	void *addr;

	if (pfn >= end_pfn) 
		panic("alloc_low_page: ran out of memory"); 

	/*
	 * Find an available temporary mapping.
	 */
	for (i = 0; temp_mappings[i].allocated; i++) {
		if (!temp_mappings[i].pmd) 
			panic("alloc_low_page: ran out of temp mappings"); 
	} 
	ti = &temp_mappings[i];
	ti->allocated = 1; 

	/*
 	 * Calculate the physical address of the new page we want to
 	 * map in temporarily.  It will be mapped at virtual address
 	 * ti->address.
 	 */
	paddr = (pfn << PAGE_SHIFT) & PMD_MASK; 

	/*
 	 * Map in the new page so that it can be accessed.
 	 */
	set_pmd(ti->pmd, __pmd(paddr | _KERNPG_TABLE | _PAGE_PSE)); 
	__flush_tlb(); 	       

	/*
 	 * Be nice and zero the page.
 	 */
	addr = ti->address + ((pfn << PAGE_SHIFT) & ~PMD_MASK); 
	memset(addr, 0, PAGE_SIZE);

	*index = i; 
	*phys  = pfn * PAGE_SIZE;  
	return addr; 
} 

/**
 * Destroy a temporary mapping that was setup by alloc_low_page().
 */
static void __init
unmap_low_page(int index)
{ 
	struct temp_map *ti;

	ti = &temp_mappings[index];
	set_pmd(ti->pmd, __pmd(0));
	ti->allocated = 0; 
} 

/**
 * Configures the input Page Middle Directory to map physical addresses
 * [address, end).  PMD entries outside of this range are zeroed.
 *
 * Each PMD table maps 1 GB of memory (512 entries, each mapping 2 MB).
 */
static void __init
phys_pmd_init(pmd_t *pmd, unsigned long address, unsigned long end)
{
	int i;

	for (i = 0; i < PTRS_PER_PMD; pmd++, i++, address += PMD_SIZE) {
		unsigned long entry;

		if (address > end) {
			for (; i < PTRS_PER_PMD; i++, pmd++)
				set_pmd(pmd, __pmd(0));
			break;
		}
		entry = _PAGE_NX|_PAGE_PSE|_KERNPG_TABLE|_PAGE_GLOBAL|address;
		entry &= __supported_pte_mask;
		set_pmd(pmd, __pmd(entry));
	}
}

/**
 * Configures the input Page Upper Directory to map physical addresses
 * [address, end).  PUD entries outside of this range are zeroed.
 *
 * Each PUD table maps 512 GB of memory (512 entries, each pointing to a PMD).
 */
static void __init
phys_pud_init(pud_t *pud, unsigned long address, unsigned long end)
{ 
	long i = pud_index(address);

	pud = pud + i;

	for (; i < PTRS_PER_PUD; pud++, i++) {
		int map; 
		unsigned long paddr, pmd_phys;
		pmd_t *pmd;

		paddr = (address & PGDIR_MASK) + i*PUD_SIZE;
		if (paddr >= end)
			break;

		if (!e820_any_mapped(paddr, paddr+PUD_SIZE, 0)) {
			set_pud(pud, __pud(0)); 
			continue;
		} 

		pmd = alloc_low_page(&map, &pmd_phys);
		set_pud(pud, __pud(pmd_phys | _KERNPG_TABLE));
		phys_pmd_init(pmd, paddr, end);
		unmap_low_page(map);
	}
	__flush_tlb();
} 

/**
 * Setup the initial kernel page tables,  These map all of physical memory
 * (0 to the top of physical memory) starting at virtual address PAGE_OFFSET.
 * This runs before bootmem is initialized and therefore has to get pages
 * directly from physical memory.
 */
void __init
init_kernel_pgtables(unsigned long start, unsigned long end)
{ 
	unsigned long next; 

	/* 
	 * Find a contiguous region of memory large enough to hold the
	 * kernel page tables.
	 */
	find_early_table_space(end);

	/*
	 * Calculate the start and end kernel virtual addresses
	 * corresponding to the input physical address range.
	 */
	start = (unsigned long)__va(start);
	end   = (unsigned long)__va(end);

	for (; start < end; start = next) {
		int map;
		unsigned long pud_phys; 
		pud_t *pud;

		/*
 		 * Allocate a new page for the Page Upper Directory.
 		 * 
		 *     pud      = kernel virtual address where the new
		 *                page can be accessed.
		 *     pud_phys = physical address of the new page.
		 *     map      = cookie needed to free the temporary mapping.
 		 */
		pud = alloc_low_page(&map, &pud_phys);

		/*
		 * Calculate the upper bound address for the PUD.
		 * The PUD maps [start, next).
		 */
		next = start + PGDIR_SIZE;
		if (next > end) 
			next = end; 

		/*
		 * Initialize the new PUD.
		 * phys_pud_init internally calls phys_pmd_init for
		 * each entry in the PUD.
		 */
		phys_pud_init(pud, __pa(start), __pa(next));

		/*
 		 * Point the Page Global Directory at the new PUD.
 		 * The PGD is the root of the page tables.
 		 */
		set_pgd(pgd_offset_k(start), mk_kernel_pgd(pud_phys));

		/* Destroy the temporary kernel mapping of the new PUD */
		unmap_low_page(map);   
	} 

	asm volatile("movq %%cr4,%0" : "=r" (mmu_cr4_features));
	__flush_tlb_all();

	printk(KERN_DEBUG
		"Allocated %lu KB for kernel page tables [0x%lx - 0x%lx)\n",
		((table_end - table_start) << PAGE_SHIFT) / 1024,
		table_start << PAGE_SHIFT,
		table_end   << PAGE_SHIFT);
}

/**
 * Destroys any virtual memory mappings from [0,PAGE_OFFSET).
 */
void __init
zap_low_mappings(int cpu)
{
	if (cpu == 0) {
		pgd_t *pgd = pgd_offset_k(0UL);
		pgd_clear(pgd);
	} else {
		/*
 		 * For AP's, zap the low identity mappings by changing the cr3
 		 * to init_level4_pgt and doing local flush tlb all.
 		 */
		asm volatile("movq %0,%%cr3" :: "r" (__pa_symbol(&init_level4_pgt)));
	}
	__flush_tlb_all();
}

