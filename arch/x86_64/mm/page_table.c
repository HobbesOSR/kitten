#include <lwk/kernel.h>
#include <lwk/bootmem.h>
#include <lwk/task.h>
#include <lwk/mm.h>
#include <arch/page.h>
#include <arch/page_table.h>

static xpte_t *
alloc_page_table(xpte_t *pte)
{
	xpte_t *new_table;

	new_table = alloc_bootmem_aligned(4096, 4096);
	if (!new_table)
		return NULL;

	if (pte) {
		xpte_t _pte;

		memset(&_pte, 0, sizeof(_pte));
		_pte.present     = 1;
		_pte.write       = 1;
		_pte.user        = 1;
		_pte.base_paddr  = __pa(new_table) >> 12;

		*pte = _pte;
	}

	return new_table;
}

static xpte_t *
find_or_create_pte(
	xpte_t        **root,
	unsigned long vaddr,
	page_size_t   pgsize
)
{
	const unsigned int pgd_index = (vaddr >> 39) & 0x1FF;
	const unsigned int pud_index = (vaddr >> 30) & 0x1FF;
	const unsigned int pmd_index = (vaddr >> 21) & 0x1FF;
	const unsigned int pte_index = (vaddr >> 12) & 0x1FF;
	xpte_t *pgd_table;
	xpte_t *pud_table;
	xpte_t *pmd_table;
	xpte_t *pte_table;
	xpte_t *pte;

	/* Allocate the Page Global Directory, if necessary */
	if (*root == NULL) {
		if ((*root = alloc_page_table(NULL)) == NULL)
			return NULL;
	}
	pgd_table = *root;

	/* Traverse the Page Global Directory */
	pte = &pgd_table[pgd_index];
	if (!pte->present && !alloc_page_table(pte))
		return NULL;
	pud_table = __va(pte->base_paddr << 12);

	/* Traverse the Page Upper Directory */
	pte = &pud_table[pud_index];
	if (pgsize == PAGE_SIZE_1GB)
		return pte;
	if (!pte->present && !alloc_page_table(pte))
		return NULL;
	if (pte->pagesize)
		panic("Can't follow PUD entry, pagesize bit set.");
	pmd_table = __va(pte->base_paddr << 12);

	/* Traverse the Page Middle Directory */
	pte = &pmd_table[pmd_index];
	if (pgsize == PAGE_SIZE_2MB)
		return pte;
	if (!pte->present && !alloc_page_table(pte))
		return NULL;
	if (pte->pagesize)
		panic("Can't follow PMD entry, pagesize bit set.");
	pte_table = __va(pte->base_paddr << 12);

	/* Traverse the Page Table Entry Directory */
	return &pte_table[pte_index];
}

void
install_page(
	xpte_t        *pte,
	unsigned long paddr,
	uint32_t      flags,
	page_size_t   pgsize
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

	if (pgsize == PAGE_SIZE_4KB) {
		_pte.base_paddr = paddr >> 12;
	} else if (pgsize == PAGE_SIZE_2MB) {
		_pte.pagesize = 1;
		_pte.base_paddr = paddr >> 21;
	} else if (pgsize == PAGE_SIZE_1GB) {
		_pte.pagesize = 1;
		_pte.base_paddr = paddr >> 30;
	} else {
		panic("Invalid page size %u.", pgsize);
	}

	*pte = _pte;
}

void
arch_map_memory(
	struct mm_struct *mm,
	unsigned long    vaddr,
	unsigned long    paddr,
	unsigned long    len,
	uint32_t         flags,
	page_size_t      pgsize
)
{
	xpte_t *root = mm->arch.page_table_root;
	xpte_t *pte;

	while (len) {
		if ((pte = find_or_create_pte(&root, vaddr, pgsize)) == NULL)
			panic("Ran out of memory allocating page table.");

		install_page(pte, paddr, flags, pgsize);

		vaddr += pgsize;
		paddr += pgsize;
		len   -= pgsize;
	}
}

