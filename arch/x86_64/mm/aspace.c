/* Copyright (c) 2007,2008 Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <lwk/init_task.h>
#include <arch/page.h>      /* TODO: remove */
#include <arch/pgtable.h>   /* TODO: remove */
#include <arch/page_table.h>


/**
 * Architecture specific address space initialization. This allocates a new
 * page table root for the aspace and copies the kernel page tables into it.
 */
int
arch_aspace_create(
	struct aspace *	aspace
)
{
	unsigned int i;

	/* Allocate a root page table for the address space */
	if ((aspace->arch.pgd = kmem_get_pages(0)) == NULL)
		return -ENOMEM;
	
	/* Copy the current kernel page tables into the address space */
	for (i = pgd_index(PAGE_OFFSET); i < PTRS_PER_PGD; i++)
		aspace->arch.pgd[i] = bootstrap_task.aspace->arch.pgd[i];

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
	unsigned int i, j, k;

	xpte_t *pgd;	/* Page Global Directory: level 0 (root of tree) */
	xpte_t *pud;	/* Page Upper Directory:  level 1 */
	xpte_t *pmd;	/* Page Middle Directory: level 2 */
	xpte_t *ptd;	/* Page Table Directory:  level 3 */

	/* Walk and then free the Page Global Directory */
	pgd = aspace->arch.pgd;
	for (i = 0; i < pgd_index(PAGE_OFFSET); i++) {
		if (!pgd[i].present)
			continue;

		/* Walk and then free the Page Upper Directory */
		pud = __va(pgd[i].base_paddr << 12);
		for (j = 0; j < 512; j++) {
			if (!pud[j].present || pud[j].pagesize)
				continue;

			/* Walk and then free the Page Middle Directory */
			pmd = __va(pud[j].base_paddr << 12);
			for (k = 0; k < 512; k++) {
				if (!pmd[k].present || pmd[k].pagesize)
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
	asm volatile(
		"movq %0,%%cr3" :: "r" (__pa(aspace->arch.pgd)) : "memory"
	);
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
		_pte.present     = 1;
		_pte.write       = 1;
		_pte.user        = 1;
		_pte.base_paddr  = __pa(new_table) >> 12;

		*parent_pte = _pte;
	}

	return new_table;
}


/**
 * Locates an existing page table entry or creates a new one if none exists.
 * Returns a pointer to the page table entry.
 */
static xpte_t *
find_or_create_pte(
	struct aspace *	aspace,
	vaddr_t		vaddr,
	vmpagesize_t	pagesz
)
{
	xpte_t *pgd;	/* Page Global Directory: level 0 (root of tree) */
	xpte_t *pud;	/* Page Upper Directory:  level 1 */
	xpte_t *pmd;	/* Page Middle Directory: level 2 */
	xpte_t *ptd;	/* Page Table Directory:  level 3 */

	xpte_t *pge;	/* Page Global Directory Entry */
	xpte_t *pue;	/* Page Upper Directory Entry */
	xpte_t *pme;	/* Page Middle Directory Entry */
	xpte_t *pte;	/* Page Table Directory Entry */

	/* Calculate indices into above directories based on vaddr specified */
	const unsigned int pgd_index = (vaddr >> 39) & 0x1FF;
	const unsigned int pud_index = (vaddr >> 30) & 0x1FF;
	const unsigned int pmd_index = (vaddr >> 21) & 0x1FF;
	const unsigned int ptd_index = (vaddr >> 12) & 0x1FF;

	/* Traverse the Page Global Directory */
	pgd = aspace->arch.pgd;
	pge = &pgd[pgd_index];
	if (!pge->present && !alloc_page_table(pge))
		return NULL;

	/* Traverse the Page Upper Directory */
	pud = __va(pge->base_paddr << 12);
	pue = &pud[pud_index];
	if (pagesz == VM_PAGE_1GB)
		return pue;
	else if (!pue->present && !alloc_page_table(pue))
		return NULL;
	else if (pue->pagesize)
		panic("BUG: Can't follow PUD entry, pagesize bit set.");

	/* Traverse the Page Middle Directory */
	pmd = __va(pue->base_paddr << 12);
	pme = &pmd[pmd_index];
	if (pagesz == VM_PAGE_2MB)
		return pme;
	else if (!pme->present && !alloc_page_table(pme))
		return NULL;
	else if (pme->pagesize)
		panic("BUG: Can't follow PMD entry, pagesize bit set.");

	/* Traverse the Page Table Entry Directory */
	ptd = __va(pme->base_paddr << 12);
	pte = &ptd[ptd_index];
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

	/* Determine if the table can be freed */
	for (i = 0; i < 512; i++) {
		if (table[i].present)
			return -1; /* Nope */
	}

	/* Yup, free the page table */
	kmem_free_pages(table, 0);
	memset(parent_pte, 0, sizeof(xpte_t));
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
	xpte_t *pgd;	/* Page Global Directory: level 0 (root of tree) */
	xpte_t *pud;	/* Page Upper Directory:  level 1 */
	xpte_t *pmd;	/* Page Middle Directory: level 2 */
	xpte_t *ptd;	/* Page Table Directory:  level 3 */

	xpte_t *pge;	/* Page Global Directory Entry */
	xpte_t *pue;	/* Page Upper Directory Entry */
	xpte_t *pme;	/* Page Middle Directory Entry */
	xpte_t *pte;	/* Page Table Directory Entry */

	/* Calculate indices into above directories based on vaddr specified */
	const unsigned int pgd_index = (vaddr >> 39) & 0x1FF;
	const unsigned int pud_index = (vaddr >> 30) & 0x1FF;
	const unsigned int pmd_index = (vaddr >> 21) & 0x1FF;
	const unsigned int ptd_index = (vaddr >> 12) & 0x1FF;

	/* Traverse the Page Global Directory */
	pgd = aspace->arch.pgd;
	pge = &pgd[pgd_index];
	if (!pge->present)
		return;

	/* Traverse the Page Upper Directory */
	pud = __va(pge->base_paddr << 12);
	pue = &pud[pud_index];
	if (!pue->present) {
		return;
	} else if (pagesz == VM_PAGE_1GB) {
		if (!pue->pagesize)
			panic("BUG: 1GB PTE has child page table attached.\n");

		/* Unmap the 1GB page that this PTE was mapping */
		memset(pue, 0, sizeof(xpte_t));

		/* Try to free PUD that the PTE was in */
		try_to_free_table(pud, pge);
		return;
	}

	/* Traverse the Page Middle Directory */
	pmd = __va(pue->base_paddr << 12);
	pme = &pmd[pmd_index];
	if (!pme->present) {
		return;
	} else if (pagesz == VM_PAGE_2MB) {
		if (!pme->pagesize)
			panic("BUG: 2MB PTE has child page table attached.\n");

		/* Unmap the 2MB page that this PTE was mapping */
		memset(pme, 0, sizeof(xpte_t));

		/* Try to free the PMD that the PTE was in */
		if (try_to_free_table(pmd, pue))
			return;  /* nope, couldn't free it */

		/* Try to free the PUD that contained the PMD just freed */
		try_to_free_table(pud, pge);
		return;
	}

	/* Traverse the Page Table Entry Directory */
	ptd = __va(pme->base_paddr << 12);
	pte = &ptd[ptd_index];
	if (pte->present) {
		return;
	} else {
		/* Unmap the 4KB page that this PTE was mapping */
		memset(pme, 0, sizeof(xpte_t));

		/* Try to free the PTD that the PTE was in */
		if (try_to_free_table(ptd, pme))
			return;  /* nope, couldn't free it */

		/* Try to free the PMD that contained the PTD just freed */
		if (try_to_free_table(pmd, pue))
			return;  /* nope, couldn't free it */

		/* Try to free the PUD that contained the PMD just freed */
		try_to_free_table(pud, pge);
		return;
	}
}


/**
 * Writes a new value to a PTE.
 * TODO: Determine if this is atomic enough.
 */
static void
write_pte(
	xpte_t *	pte,
	paddr_t		paddr,
	vmflags_t	flags,
	vmpagesize_t	pagesz
)
{
	xpte_t _pte;
	memset(&_pte, 0, sizeof(_pte));

	_pte.present = 1;
	if (flags & VM_WRITE)
		_pte.write = 1;
	if (flags & VM_USER)
		_pte.user = 1;
	if (flags & VM_GLOBAL)
		_pte.global = 1;
	if ((flags & VM_EXEC) == 0)
		_pte.no_exec = 1;

	if (pagesz == VM_PAGE_4KB) {
		_pte.base_paddr = paddr >> 12;
	} else if (pagesz == VM_PAGE_2MB) {
		_pte.pagesize = 1;
		_pte.base_paddr = paddr >> 21;
	} else if (pagesz == VM_PAGE_1GB) {
		_pte.pagesize = 1;
		_pte.base_paddr = paddr >> 30;
	} else {
		panic("Invalid page size 0x%lx.", pagesz);
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

	/* Locate page table entry that needs to be updated to map the page */
	pte = find_or_create_pte(aspace, start, pagesz);
	if (!pte)
		return -ENOMEM;

	/* Update the page table entry */
	write_pte(pte, paddr, flags, pagesz);

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
	size_t n = extent / SMARTMAP_ALIGN;
	size_t i;
	xpte_t *src_pgd = src->arch.pgd;
	xpte_t *dst_pgd = dst->arch.pgd;
	xpte_t *src_pge, *dst_pge;

	/* Make sure all of the source PGD entries are present */
	for (i = 0; i < n; i++) {
		src_pge = &src_pgd[i];
		if (!src_pge->present && !alloc_page_table(src_pge))
			return -ENOMEM;
	}

	/* Perform the SMARTMAP... just copy src PGEs to the dst PGD */
	for (i = 0; i < n; i++) {
		src_pge = &src_pgd[i];
		dst_pge = &dst_pgd[(start >> 39) & 0x1FF];
		BUG_ON(dst_pge->present);
		dst_pge = src_pge;
	}

	return 0;
}

int
arch_aspace_unsmartmap(struct aspace *src, struct aspace *dst,
                       vaddr_t start, size_t extent)
{
	size_t n = extent / SMARTMAP_ALIGN;
	size_t i;
	xpte_t *dst_pgd = dst->arch.pgd;
	xpte_t *dst_pge;

	/* Unmap the SMARTMAP PGEs */
	for (i = 0; i < n; i++) {
		dst_pge = &dst_pgd[(start >> 39) & 0x1FF];
		dst_pge->present = 0;
	}

	return 0;
}

int
arch_aspace_virt_to_phys(struct aspace *aspace, vaddr_t vaddr, paddr_t *paddr)
{
	xpte_t *pgd;	/* Page Global Directory: level 0 (root of tree) */
	xpte_t *pud;	/* Page Upper Directory:  level 1 */
	xpte_t *pmd;	/* Page Middle Directory: level 2 */
	xpte_t *ptd;	/* Page Table Directory:  level 3 */

	xpte_t *pge;	/* Page Global Directory Entry */
	xpte_t *pue;	/* Page Upper Directory Entry */
	xpte_t *pme;	/* Page Middle Directory Entry */
	xpte_t *pte;	/* Page Table Directory Entry */

	paddr_t result; /* The result of the translation */

	/* Calculate indices into above directories based on vaddr specified */
	const unsigned int pgd_index = (vaddr >> 39) & 0x1FF;
	const unsigned int pud_index = (vaddr >> 30) & 0x1FF;
	const unsigned int pmd_index = (vaddr >> 21) & 0x1FF;
	const unsigned int ptd_index = (vaddr >> 12) & 0x1FF;

	/* Traverse the Page Global Directory */
	pgd = aspace->arch.pgd;
	pge = &pgd[pgd_index];
	if (!pge->present)
		return -ENOENT;

	/* Traverse the Page Upper Directory */
	pud = __va(pge->base_paddr << 12);
	pue = &pud[pud_index];
	if (!pue->present)
		return -ENOENT;
	if (pue->pagesize) {
		result = (pue->base_paddr << 30) | (vaddr & 0x3FFFFFFF);
		goto out;
	}

	/* Traverse the Page Middle Directory */
	pmd = __va(pue->base_paddr << 12);
	pme = &pmd[pmd_index];
	if (!pme->present)
		return -ENOENT;
	if (pme->pagesize) {
		result = (pme->base_paddr << 21) | (vaddr & 0x1FFFFF);
		goto out;
	}

	/* Traverse the Page Table Entry Directory */
	ptd = __va(pme->base_paddr << 12);
	pte = &ptd[ptd_index];
	if (!pte->present)
		return -ENOENT;
	result = (pte->base_paddr << 12) | (vaddr & 0xFFF);

out:
	if (paddr)
		*paddr = result;
	return 0;
}
