/* Copyright (c) 2007,2008 Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <lwk/bootstrap.h>
#include <arch/page.h>      /* TODO: remove */
#include <arch/pgtable.h>   /* TODO: remove */
#include <arch/page_table.h>
#include <arch/tlbflush.h>


#ifdef CONFIG_ARM64_64K_PAGES
#error "64KB pages not supported. Aspace needs to be fully re-designed to work."
#endif

extern bool _can_print;
/**
 * Architecture specific address space initialization. This allocates a new
 * page table root for the aspace and copies the kernel page tables into it.
 */
int
arch_aspace_create(
	struct aspace *	aspace
)
{
	// Creates only a new user page table
	printk("Kernel page tables do not need to be copied on ARM\n");
	if ((aspace->arch.pgd = kmem_get_pages(0)) == NULL)
		return -ENOMEM;
	return 0;
}


/**
 * Architecture specific address space destruction. This frees all page table
 * memory that the aspace was using.
 */
void
arch_aspace_destroy(
	struct aspace *	aspace
)
{
	panic("arch_aspace_destroy\n");
#if 0
	unsigned int i, j, k;

	xpte_t *pgd;	/* Page Global Directory: level 0 (root of tree) */
	xpte_t *pud;	/* Page Upper Directory:  level 1 */
	xpte_t *pmd;	/* Page Middle Directory: level 2 */
	xpte_t *ptd;	/* Page Table Directory:  level 3 */

	/* Walk and then free the Page Global Directory */
	pgd = aspace->arch.pgd;
	for (i = 0; i < pgd_index(PAGE_OFFSET); i++) {
		if (!pgd[i].valid)
			continue;

		/* Walk and then free the Page Upper Directory */
		pud = __va(pgd[i].base_paddr << 12);
		for (j = 0; j < 512; j++) {
			if (!pud[j].valid || !pud[j].type)
				continue;

			/* Walk and then free the Page Middle Directory */
			pmd = __va(pud[j].base_paddr << 12);
			for (k = 0; k < 512; k++) {
				if (!pmd[k].valid || !pmd[k].type)
					continue;
				
				/* Free the last level Page Table Directory */
				ptd = __va(pmd[k].base_paddr << 12);
				kmem_free_pages(ptd, 0);
			}
			kmem_free_pages(pmd, 0);
		}
		kmem_free_pages(pud, 0);
	}
	kmem_free_pages(pgd, 0);
#endif
}


/**
 * Loads the address space object's root page table pointer into the calling
 * CPU's CR3 register, causing the aspace to become active.
 */
void
arch_aspace_activate(
	struct aspace *	aspace
)
{
	printk("activate aspace %p\n",aspace->arch.pgd);
	//printk("aspace->child_list %p\n",aspace->child_list);
	//printk("&aspace->child_list %p\n",&aspace->child_list);
	//printk("(&aspace->child_list)->prev %p\n",(&aspace->child_list)->prev);
	asm volatile(
			"dsb #0\n"
			"msr TTBR0_EL1, %0\n"
			"isb\n":: "r" (__pa(aspace->arch.pgd)) : "memory");
}


/**
 * Allocates a new page table and links it to a parent page table entry.
 */
static xpte_t *
alloc_page_table(
	xpte_t *	parent_pte
)
{
	xpte_t *new_table;

	new_table = kmem_get_pages(0);
	if (!new_table)
		return NULL;
	
	if (parent_pte) {
		xpte_t _pte;

		memset(&_pte, 0, sizeof(_pte));
		_pte.valid     = 1;
		_pte.type      = 1;


#if 0
		_pte.AF        = 1;
		// NOTE: original code set write to be 1 for x86-64 on ARM64 AP2 should be 0
		_pte.AP2         = 0;
		// NOTE: original code set user to be 1 for x86-64 on ARM64 AP1 should also be set to 1
		_pte.AP1        = 1;
#endif

		_pte.base_paddr  = __pa(new_table) >> PAGE_SHIFT;

		*parent_pte = _pte;
	}

	return new_table;
}

/**
 * Locates an existing page table entry or creates a new one if none exists.
 * Returns a pointer to the page table entry.
 */
static xpte_leaf_t *
find_or_create_pte(
	struct aspace *	aspace,
	vaddr_t		vaddr,
	vmpagesize_t	pagesz
)
{

	xpte_t *pgd = NULL;	/* Page Global Directory: level 1 (root of tree) */
	xpte_t *pmd = NULL;	/* Page Middle Directory: level 2 */
	xpte_t *ptd = NULL;	/* Page Table Directory:  level 3 */

	xpte_t *pge = NULL;	/* Page Global Directory Entry */
	xpte_t *pue = NULL;	/* Page Upper Directory Entry */
	xpte_t *pme = NULL;	/* Page Middle Directory Entry */
	xpte_t *pte = NULL;	/* Page Table Directory Entry */

	/* Calculate indices into above directories based on vaddr specified */
	unsigned int pgd_index =  (vaddr >> 30) & 0x1FF;
	unsigned int pmd_index =  (vaddr >> 21) & 0x1FF;
	unsigned int ptd_index =  (vaddr >> 12) & 0x1FF;



	/* JRL: These should either be handled OK here, or be moved to initialization time checks */
	{
		tcr_el1 tcr = get_tcr_el1();

		if (vaddr >= 0xffffff8000000000ull) {
			panic("Unable to handle Kernel page translations\n");
		} else if ((tcr.t0sz < 25) || (tcr.t0sz > 33)) {
			panic("Unable to handle this starting level\n");
		} else if (aspace->arch.pgd == NULL) {
			panic("Aspace has NULL PGD\n");
		}
	}

	/* Traverse the Page Global Directory */
	pgd = aspace->arch.pgd;
	pge = &pgd[pgd_index];
	if (pagesz == VM_PAGE_1GB) {
		return pge;
	} else if (!pge->valid && !alloc_page_table(pge)) {
		return NULL;
	} else if (!pge->type) {
		panic("BUG: Can't follow PGD entry, type is Block.");
	}

	/* Traverse the Page Middle Directory */
	pmd = __va(xpte_paddr(pge));
	pme = &pmd[pmd_index];
	if (pagesz == VM_PAGE_2MB) {
		return pme;
	} else if (!pme->valid && !alloc_page_table(pme)) {
		return NULL;
	} else if (!pme->type) {
		panic("BUG: Can't follow PMD entry, type is Block.");
	}
	

	ptd = __va(xpte_paddr(pme));
	pte = &ptd[ptd_index];
out:
	return pte;
}


/**
 * Examines a page table to determine if it has any active entries. If not,
 * the page table is freed.
 */
static int
try_to_free_table(
	xpte_t *	table,
	xpte_t *	parent_pte
)
{
	int i;

	//printk("table %p parent %p\n",table,parent_pte);
	/* Determine if the table can be freed */
	for (i = 0; i < 512; i++) {
		if (table[i].valid)
			return -1; /* Nope */
	}

	//printk("freed! %p\n",table);
	/* Yup, free the page table */
	kmem_free_pages(table, 0);
	if (parent_pte != NULL) {
		memset(parent_pte, 0, sizeof(xpte_t));
	}
	return 0;
}


/**
 * Zeros a page table entry. If the page table that the PTE was in becomes
 * empty (contains no active mappings), it is freed. Page table freeing
 * continues up to the top of the page table tree (e.g., a single call may
 * result in a PTD, PMD, and PUD being freed; the PGD is never freed by this
 * function).
 */
static void
find_and_delete_pte(
	struct aspace *	aspace,
	vaddr_t		vaddr,
	vmpagesize_t	pagesz
)
{
	xpte_t *pgd = NULL;	/* Page Global Directory: level 1 (root of tree) */
	xpte_t *pmd = NULL;	/* Page Middle Directory: level 2 */
	xpte_t *ptd = NULL;	/* Page Table Directory:  level 3 */

	xpte_t *pge = NULL;	/* Page Global Directory Entry */
	xpte_t *pme = NULL;	/* Page Middle Directory Entry */
	xpte_t *pte = NULL;	/* Page Table Directory Entry */


	/* Calculated Assuming a 4KB granule */
	unsigned int pgd_index = (vaddr >> 30) & 0x1FF;
	unsigned int pmd_index = (vaddr >> 21) & 0x1FF;
	unsigned int ptd_index = (vaddr >> 12) & 0x1FF;



	/* JRL: These should either be handled OK here, or be moved to initialization time checks */
	{
		tcr_el1 tcr = get_tcr_el1();

		if (vaddr >= 0xffffff8000000000ull) {
			panic("Unable to handle Kernel page translations\n");
		} else if ((tcr.t0sz < 25) || (tcr.t0sz > 33)) {
			panic("Unable to handle this starting level\n");
		} else if (aspace->arch.pgd == NULL) {
			panic("Aspace has NULL PGD\n");
		}

	}


	pgd = aspace->arch.pgd;
	pge = &pgd[pgd_index];

	/* Traverse the Page Global Directory */
	if (!pge->valid) {
		return;
	} else if (pagesz == VM_PAGE_1GB) {
		if (pge->type) {
			panic("BUG: 1GB PTE has child page table attached.\n");
		}

		/* Unmap the 1GB page that this PTE was mapping */
		memset(pge, 0, sizeof(xpte_t));

		return;
	}



	/* Traverse the Page Middle Directory */
	pmd = __va(xpte_paddr(pge));
	pme = &pmd[pmd_index];
	if (!pme->valid) {
		return;
	} else if (pagesz == VM_PAGE_2MB) {
		if (pme->type) {
			panic("BUG: 2MB PTE has child page table attached.\n");
		}

		/* Unmap the 2MB page that this PTE was mapping */
		memset(pme, 0, sizeof(xpte_t));

		/* Try to free the PUD that contained the PMD just freed */
		try_to_free_table(pmd, pge);
		return;
	}

	/* Traverse the Page Table Entry Directory */
	ptd = __va(xpte_paddr(pme));
	pte = &ptd[ptd_index];
	if (!pte->valid) {
		return;
	} else {
		/* Unmap the 4KB page that this PTE was mapping */
		memset(pte, 0, sizeof(xpte_t));

		/* Try to free the PTD that the PTE was in */
		if (try_to_free_table(ptd, pme))
			return;  /* nope, couldn't free it */

		/* Try to free the PGD that contained the PMD just freed */
		try_to_free_table(pmd, pge);
		return;
	}
	printk("6\n");
}


/**
 * Writes a new value to a PTE.
 * TODO: Determine if this is atomic enough.
 */
static void
write_pte(
	xpte_leaf_t *	pte,
	paddr_t		paddr,
	vmflags_t	flags,
	vmpagesize_t	pagesz
)
{
	xpte_leaf_t _pte;  
	memset(&_pte, 0, sizeof(_pte));

	_pte.valid = 1;
	_pte.AF    = 1;
	
	if (flags & VM_WRITE) {
		_pte.AP2 = 0;
	} else {
		_pte.AP2 = 1;
	}

	if (flags & VM_USER) {
		_pte.AP1 = 1;
	}

	if (flags & VM_GLOBAL) {
		_pte.nG = 0;
	} else {
		_pte.nG = 1;
	}

	if ((flags & VM_EXEC) == 0) {
		_pte.PXN = 1;
		_pte.XN  = 1;
	}

	if (pagesz == VM_PAGE_4KB) {
		xpte_4KB_t * _pte_4kb = &_pte;
		_pte_4kb->base_paddr  = paddr >> PAGE_SHIFT_4KB;
		_pte_4kb->type        = 1;
	} else if (pagesz == VM_PAGE_2MB) {
		xpte_2MB_t * _pte_2mb = &_pte;
		_pte_2mb->base_paddr  = paddr >> PAGE_SHIFT_2MB;
		_pte_2mb->type        = 0;
	} else if (pagesz == VM_PAGE_1GB) {
		xpte_1GB_t * _pte_1gb = &_pte;
		_pte_1gb->base_paddr  = paddr >> PAGE_SHIFT_1GB;
		_pte_1gb->type        = 0;

	}

	*pte = _pte;
}


/**
 * Maps a page into an address space.
 *
 * Arguments:
 *       [IN] aspace: Address space to map page into.
 *       [IN] start:  Address in aspace to map page to.
 *       [IN] paddr:  Physical address of the page to map.
 *       [IN] flags:  Protection and memory type flags.
 *       [IN] pagesz: Size of the page being mapped, in bytes.
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, the page was not mapped.
 */
int
arch_aspace_map_page(
	struct aspace *	aspace,
	vaddr_t		start,
	paddr_t		paddr,
	vmflags_t	flags,
	vmpagesize_t	pagesz
)
{
	xpte_t *pte;

	printk("Mapping Page (vaddr=%p) (paddr=%p) (pagesz=%d)\n", (void *)start, (void *)paddr, pagesz);

	/* Locate page table entry that needs to be updated to map the page */
	pte = find_or_create_pte(aspace, start, pagesz);
	if (!pte)
		return -ENOMEM;

	/* Update the page table entry */
	write_pte(pte, paddr, flags, pagesz);

	// TODO: Need to narrow down the page the entries that are being updated
	flush_cache_all();
	flush_tlb_all();
	return 0;
}


/**
 * Unmaps a page from an address space.
 *
 * Arguments:
 *       [IN] aspace: Address space to unmap page from.
 *       [IN] start:  Address in aspace to unmap page from.
 *       [IN] pagesz: Size of the page to unmap.
 */
void
arch_aspace_unmap_page(
	struct aspace *	aspace,
	vaddr_t		start,
	vmpagesize_t	pagesz
)
{
	find_and_delete_pte(aspace, start, pagesz);
}

int
arch_aspace_smartmap(struct aspace *src, struct aspace *dst,
                     vaddr_t start, size_t extent)
{
	panic("arch aspace smartmap\n");
#if 0
	size_t n = extent / SMARTMAP_ALIGN;
	size_t i;
	xpte_t *src_pgd = src->arch.pgd;
	xpte_t *dst_pgd = dst->arch.pgd;
	xpte_t *src_pge, *dst_pge;

	printk("arch_aspace_smartmap: %llx\n",start);
	/* Make sure all of the source PGD entries are present */
	for (i = 0; i < n; i++) {
		src_pge = &src_pgd[i];
		if (!src_pge->valid && !alloc_page_table(src_pge))
			return -ENOMEM;
	}

	/* Perform the SMARTMAP... just copy src PGEs to the dst PGD */
	for (i = 0; i < n; i++) {
		src_pge = &src_pgd[i];
		dst_pge = &dst_pgd[(start >> 39) & 0x1FF];
		BUG_ON(dst_pge->valid);
		*dst_pge = *src_pge;
	}
#endif
	return 0;
}

int
arch_aspace_unsmartmap(struct aspace *src, struct aspace *dst,
                       vaddr_t start, size_t extent)
{
	panic("arch aspace unsmartmap\n");
#if 0
	size_t n = extent / SMARTMAP_ALIGN;
	size_t i;
	xpte_t *dst_pgd = dst->arch.pgd;
	xpte_t *dst_pge;

	/* Unmap the SMARTMAP PGEs */
	for (i = 0; i < n; i++) {
		dst_pge = &dst_pgd[(start >> 39) & 0x1FF];
		dst_pge->valid = 0;
		dst_pge->AF = 0;
	}
#endif
	return 0;
}

extern bool _can_print;

int
arch_aspace_virt_to_phys(struct aspace *aspace, vaddr_t vaddr, paddr_t *paddr)
{
	xpte_t *pgd = NULL;	/* Page Global Directory: level 0 (root of tree) */
	xpte_t *pud = NULL;	/* Page Upper Directory:  level 1 */
	xpte_t *pmd = NULL;	/* Page Middle Directory: level 2 */
	xpte_t *ptd = NULL;	/* Page Table Directory:  level 3 */

	xpte_t *pge;	/* Page Global Directory Entry */
	xpte_t *pue;	/* Page Upper Directory Entry */
	xpte_t *pme;	/* Page Middle Directory Entry */
	xpte_t *pte;	/* Page Table Directory Entry */

	paddr_t result; /* The result of the translation */

	/* Calculate indices into above directories based on vaddr specified */
	unsigned int pgd_index = (vaddr >> 39) & 0x1FF;
	unsigned int pud_index = (vaddr >> 30) & 0x1FF;
	unsigned int pmd_index = (vaddr >> 21) & 0x1FF;
	unsigned int ptd_index = (vaddr >> 12) & 0x1FF;

	tcr_el1 tcr;

	tcr = get_tcr_el1();

	if (tcr.t0sz >= 25 && tcr.t0sz <= 33) {
		// Start at level 1
		// TODO Brian
		if (vaddr < 0xffffff8000000000ul)
			pud = aspace->arch.pgd;
		else
			pud = __phys_to_virt(get_ttbr1_el1());
	} else {
		printk("Unable to handle this starting level\n");
		goto out;
	}

	if (tcr.tg0 == 0 && tcr.tg1 == 2) {
		// 4k granule
		pgd_index = (vaddr >> 39) & 0x1FF;
		pud_index = (vaddr >> 30) & 0x1FF;
		pmd_index = (vaddr >> 21) & 0x1FF;
		ptd_index = (vaddr >> 12) & 0x1FF;
	} else {
		printk("Unable to handle non-4k granule sizes\n");
		goto out;
	}

	/* Traverse the Page Global Directory */
	if (pgd != NULL) {
		pge = &pgd[pgd_index];
		if (!pge->valid)
			return -ENOENT;
		pud = __va(pge->base_paddr << 12);
	}
	/* Traverse the Page Upper Directory */
	if (pud != NULL) {
		pue = &pud[pud_index];
		if (!pue->valid)
			return -ENOENT;
		if (!pue->type) {
			result = ((uint64_t)(((xpte_1GB_t *)pue)->base_paddr << 30))
		        				 | (vaddr & 0x3FFFFFFFull);
			goto out;
		}
		pmd = __va(pue->base_paddr << 12);
	}

	/* Traverse the Page Middle Directory */
	if (pmd != NULL) {
		pme = &pmd[pmd_index];
		if (!pme->valid)
			return -ENOENT;
		if (!pme->type) {
			result = ((uint64_t)(((xpte_2MB_t *)pme)->base_paddr) << 21)
		        						 | (vaddr & 0x1FFFFFull);
			goto out;
		}
	}

	/* Traverse the Page Table Entry Directory */
	ptd = __va(pme->base_paddr << 12);
	pte = &ptd[ptd_index];
	if (!pte->valid)
		return -ENOENT;
	result = ((uint64_t)(((xpte_4KB_t *)pte)->base_paddr) << 12)
	        		 | (vaddr & 0xFFFull);

	out:
	if (paddr)
		*paddr = result;
	return 0;
}


/**
 * This maps a region of physical memory into the kernel virtual address space.
 * It assumes start and end are aligned to a 2 MB boundary and that the
 * kernel is using 2 MB pages to map physical memory into the kernel virtual
 * address space.
 */
int
arch_aspace_map_pmem_into_kernel(paddr_t start, paddr_t end)
{
	paddr_t paddr;
	int status;

	//printk("arch_aspace_map_pmem_into_kernel\n");
	for (paddr = start; paddr < end; paddr += VM_PAGE_2MB) {
		/* If the page isn't already mapped, we need to map it */
		if (arch_aspace_virt_to_phys(&bootstrap_aspace, (vaddr_t)__va(paddr), NULL) == -ENOENT) {
			printk(KERN_INFO "Missing kernel memory found, paddr=0x%016lx.\n", paddr);

			status =
			arch_aspace_map_page(
				&bootstrap_aspace,
				(vaddr_t)__va(paddr),
				paddr,
				VM_KERNEL,
				VM_PAGE_2MB
			);
			printk("pmem_into\n");

			if (status) {
				printk(KERN_ERR "Could not map kernel memory for paddr=0x%016lx.\n", paddr);
				printk(KERN_ERR "Kernel address space is now inconsistent.\n");
				return -1;
			}
		}
	}
	return 0;
}

/**
 * This unmaps a region of physical memory from the kernel virtual address
 * space.  It assumes start and end are aligned to a 2 MB boundary and that the
 * kernel is using 2 MB pages to map physical memory into the kernel virtual
 * address space.
 */

int
arch_aspace_unmap_pmem_from_kernel(paddr_t start, paddr_t end)
{
	paddr_t paddr;

	for (paddr = start; paddr < end; paddr += VM_PAGE_2MB) {
		/* If the page is mapped, we need to unmap it */
		if (arch_aspace_virt_to_phys(&bootstrap_aspace, (vaddr_t)__va(paddr), NULL) == 0) {
			arch_aspace_unmap_page(
				&bootstrap_aspace,
				(vaddr_t)__va(paddr),
				VM_PAGE_2MB
			);
		}
	}

	return 0;
}
