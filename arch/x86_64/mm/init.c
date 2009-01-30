#include <lwk/kernel.h>
#include <lwk/bootmem.h>
#include <lwk/string.h>
#include <lwk/pmem.h>
#include <arch/page.h>
#include <arch/pgtable.h>
#include <arch/e820.h> 
#include <arch/tlbflush.h>
#include <arch/proto.h>

/**
 * Start and end page frame numbers of the kernel page tables.
 */
unsigned long __initdata table_start, table_end;  /* page frame numbers */

static __init void *early_ioremap(unsigned long addr, unsigned long size)
{
	unsigned long vaddr;
	pmd_t *pmd, *last_pmd;
	int i, pmds;

	pmds = ((addr & ~PMD_MASK) + size + ~PMD_MASK) / PMD_SIZE;
	vaddr = __START_KERNEL_map;
	pmd = level2_kernel_pgt;
	last_pmd = level2_kernel_pgt + PTRS_PER_PMD - 1;
	for (; pmd <= last_pmd; pmd++, vaddr += PMD_SIZE) {
		for (i = 0; i < pmds; i++) {
			if (pmd_present(pmd[i]))
				goto next;
		}
		vaddr += addr & ~PMD_MASK;
		addr &= PMD_MASK;
		for (i = 0; i < pmds; i++, addr += PMD_SIZE)
			set_pmd(pmd + i,__pmd(addr | _KERNPG_TABLE | _PAGE_PSE));
		__flush_tlb();
		return (void *)vaddr;
	next:
		;
	}
	printk("early_ioremap(0x%lx, %lu) failed\n", addr, size);
	return NULL;
}

/* To avoid virtual aliases later */
static __init void early_iounmap(void *addr, unsigned long size)
{
	unsigned long vaddr;
	pmd_t *pmd;
	int i, pmds;

	vaddr = (unsigned long)addr;
	pmds = ((vaddr & ~PMD_MASK) + size + ~PMD_MASK) / PMD_SIZE;
	pmd = level2_kernel_pgt + pmd_index(vaddr);
	for (i = 0; i < pmds; i++)
		pmd_clear(pmd + i);
	__flush_tlb();
}

static __init void *alloc_low_page(unsigned long *phys)
{ 
	unsigned long pfn = table_end++;
	void *adr;

	if (pfn >= end_pfn) 
		panic("alloc_low_page: ran out of memory"); 

	adr = early_ioremap(pfn * PAGE_SIZE, PAGE_SIZE);
	memset(adr, 0, PAGE_SIZE);
	*phys  = pfn * PAGE_SIZE;
	return adr;
}

/**
 * Destroys a temporary mapping that was setup by alloc_low_page().
 */
static void __init unmap_low_page(void *adr)
{ 
	early_iounmap(adr, PAGE_SIZE);
} 

/**
 * Initializes a fixmap entry to point to a given physical page.
 */
void __init
__set_fixmap(
	enum fixed_addresses fixmap_index, /* fixmap entry index to setup */
	unsigned long        phys_addr,    /* map fixmap entry to this addr */
	pgprot_t             prot          /* page protection bits */
)
{
	unsigned long virt_addr;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte, new_pte;

	if (fixmap_index >= __end_of_fixed_addresses)
		panic("Invalid FIXMAP index");

	/* Calculate the virtual address of the fixmap entry */
	virt_addr = __fix_to_virt(fixmap_index);

	/* Look up PGD entry covering the fixmap entry */
	pgd = pgd_offset_k(virt_addr);
	if (pgd_none(*pgd))
		panic("PGD FIXMAP MISSING, it should be setup in head.S!\n");

	/* Look up the PMD entry covering the fixmap entry */
	pud = pud_offset(pgd, virt_addr);
	if (pud_none(*pud)) {
		/* PUD entry is empty... allocate a new PMD directory for it */
		pmd = (pmd_t *) alloc_bootmem_aligned(PAGE_SIZE, PAGE_SIZE);
		set_pud(pud, __pud(__pa(pmd) | _KERNPG_TABLE | _PAGE_USER));
		BUG_ON(pmd != pmd_offset(pud, 0));
	}

	/* Look up the PMD entry covering the fixmap entry */
	pmd = pmd_offset(pud, virt_addr);
	if (pmd_none(*pmd)) {
		/* PMD entry is empty... allocate a new PTE directory for it */
		pte = (pte_t *) alloc_bootmem_aligned(PAGE_SIZE, PAGE_SIZE);
		set_pmd(pmd, __pmd(__pa(pte) | _KERNPG_TABLE | _PAGE_USER));
		BUG_ON(pte != pte_offset_kernel(pmd, 0));
	}

	/*
	 * Construct and install a new PTE that maps the fixmap entry
	 * to the requested physical address.
	 */
	pte     = pte_offset_kernel(pmd, virt_addr);
	new_pte = pfn_pte(phys_addr >> PAGE_SHIFT, prot);
	set_pte(pte, new_pte);
	__flush_tlb_one(virt_addr);
}

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
		unsigned long paddr, pmd_phys;
		pmd_t *pmd;

		paddr = (address & PGDIR_MASK) + i*PUD_SIZE;
		if (paddr >= end)
			break;

		if (!e820_any_mapped(paddr, paddr+PUD_SIZE, 0)) {
			set_pud(pud, __pud(0)); 
			continue;
		} 

		pmd = alloc_low_page(&pmd_phys);
		set_pud(pud, __pud(pmd_phys | _KERNPG_TABLE));
		phys_pmd_init(pmd, paddr, end);
		unmap_low_page(pmd);
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
		pud = alloc_low_page(&pud_phys);

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
		unmap_low_page(pud);   
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
 * This performs architecture-specific memory subsystem initialization. It is
 * called from the platform-independent memsys_init(). For x86_64, the only
 * thing that needs to be done is to relocate the initrd image to user memory.
 */
void __init
arch_memsys_init(size_t kmem_size)
{
	struct pmem_region query, result;
	size_t initrd_size, umem_size;
	paddr_t new_initrd_start, new_initrd_end;

	if (!initrd_start)
		return;

	initrd_size = initrd_end - initrd_start;
	umem_size   = round_up(initrd_size, PAGE_SIZE);

	/* Relocate the initrd image to an unallocated chunk of umem */
	if (pmem_alloc_umem(umem_size, PAGE_SIZE, &result))
		panic("Failed to allocate umem for initrd image.");
	result.type = PMEM_TYPE_INITRD;
	pmem_update(&result);
	memmove(__va(result.start), __va(initrd_start), initrd_size);

	new_initrd_start = result.start;
	new_initrd_end   = new_initrd_start + initrd_size;

	/* Free the memory used by the old initrd location */
	pmem_region_unset_all(&query);
	query.start = round_down( initrd_start, PAGE_SIZE );
	query.end   = round_up(   initrd_end,   PAGE_SIZE );
	while (pmem_query(&query, &result) == 0) {
		result.type = (result.start < kmem_size) ? PMEM_TYPE_KMEM
		                                         : PMEM_TYPE_UMEM;
		result.allocated = false;
		BUG_ON(pmem_update(&result));
		query.start = result.end;
	}
	
	/* Update initrd_start and initrd_end to their new values */
	initrd_start = new_initrd_start;
	initrd_end   = new_initrd_end;
}

