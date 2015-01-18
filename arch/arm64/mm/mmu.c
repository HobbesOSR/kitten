/*
 * Based on arch/arm/mm/mmu.c
 *
 * Copyright (C) 1995-2005 Russell King
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <arch/memblock.h>
#include <arch/cacheflush.h>
#include <arch/tlbflush.h>
#include <arch/pgalloc.h>
#include <arch/pgtable.h>
#include <lwk/aspace.h>
#include <lwk/pfn.h>

struct aspace init_mm;
#if 0
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/mman.h>
#include <linux/nodemask.h>
#include <linux/memblock.h>
#include <linux/fs.h>
#include <linux/io.h>

#include <asm/cputype.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/sizes.h>
#include <asm/tlb.h>
#include <asm/mmu_context.h>

#include "mm.h"

/*
 * Empty_zero_page is a special page that is used for zero-initialized data
 * and COW.
 */
struct page *empty_zero_page;
EXPORT_SYMBOL(empty_zero_page);

EXPORT_SYMBOL(pgprot_default);
#endif

pgprot_t pgprot_default;

static pmdval_t prot_sect_kernel;

void
print_tables_arm64(void* addr)
{
	uint64_t ttbr1=get_ttbr1_el1();
	uint64_t phys = ttbr1 & ((-1ull) << 12);
	uint64_t *gig = __va(phys);
	uint64_t *meg2= NULL;
	int x = 0;
	int y = 0;


	// Gig pages
	printk("TTBR1 Tables %p %p\n",gig,addr);
	printk("Entry %llx\n",*gig);
	for(x = 0; x < 512; x++) {
		if (((((unsigned long)addr >> 30) & 0x1fful) == x) || addr == NULL)
			printk("%03x: %016llx\n",x,gig[x]);
		if (((gig[x] & 3u) == 3u) && (((((unsigned long)addr >> 30) & 0x1ff) == x) || addr == NULL)) {
			meg2 = __va(gig[x] & (-1ull << 12));
			for (y = 0; y < 512; y++) {
				if (((meg2[y] & 3u) != 0) && (((((unsigned long)addr >> 21) & 0x1ff) == y) || addr == NULL)) {
					printk("\t%03x: %016llx\n",y,meg2[y]);
				}
			}
		}
	}
	printk("done\n");
	ttbr1 = get_ttbr0_el1();
	phys = ttbr1 & ((-1ull) << 12);
	gig = __va(phys);
	meg2= NULL;
	x = 0;
	y = 0;


	// Gig pages
	printk("TTBR0 Tables %p\n",gig);
	for(x = 0; x < 512; x++) {
		if ((gig[x] & 3u) == 3u && ((((unsigned long)addr >> 30) & 0x1ff == x) || addr == NULL)) {
			printk("%03x: %016llx\n",x,gig[x]);
			meg2 = __va(gig[x] & (-1ull << 12));
			for (y = 0; y < 512; y++) {
				if ((meg2[y] & 3u && ((((unsigned long)addr >> 21) & 0x1ff == y) || addr == NULL)) != 0) {
					printk("\t%03x: %016llx\n",y,meg2[y]);
				}
			}
		}
	}
	printk("done\n");
}
//EXPORT_SYMBOL_GPL(print_tables_arm64);

#if 0
struct cachepolicy {
	const char	policy[16];
	u64		mair;
	u64		tcr;
};

static struct cachepolicy cache_policies[] __initdata = {
	{
		.policy		= "uncached",
		.mair		= 0x44,			/* inner, outer non-cacheable */
		.tcr		= TCR_IRGN_NC | TCR_ORGN_NC,
	}, {
		.policy		= "writethrough",
		.mair		= 0xaa,			/* inner, outer write-through, read-allocate */
		.tcr		= TCR_IRGN_WT | TCR_ORGN_WT,
	}, {
		.policy		= "writeback",
		.mair		= 0xee,			/* inner, outer write-back, read-allocate */
		.tcr		= TCR_IRGN_WBnWA | TCR_ORGN_WBnWA,
	}
};

/*
 * These are useful for identifying cache coherency problems by allowing the
 * cache or the cache and writebuffer to be turned off. It changes the Normal
 * memory caching attributes in the MAIR_EL1 register.
 */
static int __init early_cachepolicy(char *p)
{
	int i;
	u64 tmp;

	for (i = 0; i < ARRAY_SIZE(cache_policies); i++) {
		int len = strlen(cache_policies[i].policy);

		if (memcmp(p, cache_policies[i].policy, len) == 0)
			break;
	}
	if (i == ARRAY_SIZE(cache_policies)) {
		pr_err("ERROR: unknown or unsupported cache policy: %s\n", p);
		return 0;
	}

	flush_cache_all();

	/*linux
	 * Modify MT_NORMAL attributes in MAIR_EL1.
	 */
	asm volatile(
	"	mrs	%0, mair_el1\n"
	"	bfi	%0, %1, #%2, #8\n"
	"	msr	mair_el1, %0\n"
	"	isb\n"
	: "=&r" (tmp)
	: "r" (cache_policies[i].mair), "i" (MT_NORMAL * 8));

	/*
	 * Modify TCR PTW cacheability attributes.
	 */
	asm volatile(
	"	mrs	%0, tcr_el1\n"
	"	bic	%0, %0, %2\n"
	"	orr	%0, %0, %1\n"
	"	msr	tcr_el1, %0\n"
	"	isb\n"
	: "=&r" (tmp)
	: "r" (cache_policies[i].tcr), "r" (TCR_IRGN_MASK | TCR_ORGN_MASK));

	flush_cache_all();

	return 0;
}
early_param("cachepolicy", early_cachepolicy);
/*
 * Adjust the PMD section entries according to the CPU in use.
 */
#endif
static void __init init_mem_pgprot(void)
{
	pteval_t default_pgprot;
	int i;

	default_pgprot = PTE_ATTRINDX(MT_NORMAL);
	prot_sect_kernel = PMD_TYPE_SECT | PMD_SECT_AF | PMD_ATTRINDX(MT_NORMAL);

#ifdef CONFIG_SMP
	/*
	 * Mark memory with the "shared" attribute for SMP systems
	 */
	default_pgprot |= PTE_SHARED;
	prot_sect_kernel |= PMD_SECT_S;
#endif

#if 0
	for (i = 0; i < 16; i++) {
		unsigned long v = pgprot_val(protection_map[i]);
		protection_map[i] = __pgprot(v | default_pgprot);
	}
#endif

	pgprot_default = __pgprot(PTE_TYPE_PAGE | PTE_AF | default_pgprot);
}

#if 0
pgprot_t phys_mem_access_prot(struct file *file, unsigned long pfn,
			      unsigned long size, pgprot_t vma_prot)
{
	if (!pfn_valid(pfn))
		return pgprot_noncached(vma_prot);
	else if (file->f_flags & O_SYNC)
		return pgprot_writecombine(vma_prot);
	return vma_prot;
}
EXPORT_SYMBOL(phys_mem_access_prot);
#endif
static void __init *early_alloc(unsigned long sz)
{
	void *ptr = __va(memblock_alloc(sz, sz));
	memset(ptr, 0, sz);
	return ptr;
}

static void __init alloc_init_pte(pmd_t *pmd, unsigned long addr,
				  unsigned long end, unsigned long pfn)
{
	pte_t *pte;
	if (pmd_none(*pmd)) {
		pte = early_alloc(PTRS_PER_PTE * sizeof(pte_t));
		__pmd_populate(pmd, __pa(pte), PMD_TYPE_TABLE);
	}
	BUG_ON(pmd_bad(*pmd));

	pte = pte_offset_kernel(pmd, addr);
	do {
		set_pte(pte, pfn_pte(pfn, PAGE_KERNEL_EXEC));
		pfn++;
	} while (pte++, addr += PAGE_SIZE, addr != end);
}

static void __init alloc_init_pmd(pud_t *pud, unsigned long addr,
				  unsigned long end, phys_addr_t phys)
{
	pmd_t *pmd;
	unsigned long next;

	/*
	 * Check for initial section mappings in the pgd/pud and remove them.
	 */
	if (pud_none(*pud) || pud_bad(*pud)) {
		pmd = early_alloc(PTRS_PER_PMD * sizeof(pmd_t));
		pud_populate(&init_mm, pud, pmd);
	}

	pmd = pmd_offset(pud, addr);
	do {
		next = pmd_addr_end(addr, end);
		/* try section mapping first */
		if (((addr | next | phys) & ~SECTION_MASK) == 0) {
			set_pmd(pmd, __pmd(phys | prot_sect_kernel));
		} else
			alloc_init_pte(pmd, addr, next, PFN_DOWN(phys));
			// TODO: Brian // alloc_init_pte(pmd, addr, next, __phys_to_pfn(phys));
		phys += next - addr;
	} while (pmd++, addr = next, addr != end);
}

static void __init alloc_init_pud(pgd_t *pgd, unsigned long addr,
				  unsigned long end, unsigned long phys)
{
	pud_t *pud = pud_offset(pgd, addr);
	unsigned long next;

	do {
		next = pud_addr_end(addr, end);
		alloc_init_pmd(pud, addr, next, phys);
		phys += next - addr;
	} while (pud++, addr = next, addr != end);
}

/*
 * Create the page directory entries and any necessary page tables for the
 * mapping specified by 'md'.
 */
static void __init create_mapping(phys_addr_t phys, unsigned long virt,
                                  phys_addr_t size)
{
        unsigned long addr, length, end, next;
        pgd_t *pgd;

        if (virt < VMALLOC_START) {
                /*pr_warning("BUG: not creating mapping for 0x%016llx at 0x%016lx - outside kernel range\n",
                           phys, virt);*/
                return;
        }

        addr = virt & PAGE_MASK;
        length = PAGE_ALIGN(size + (virt & ~PAGE_MASK));

        pgd = pgd_offset_k(addr);
        end = addr + length;
        do {
                next = pgd_addr_end(addr, end);
                alloc_init_pud(pgd, addr, next, phys);
                phys += next - addr;
        } while (pgd++, addr = next, addr != end);
}


#if 0
#ifdef CONFIG_EARLY_PRINTK
/*
 * Create an early I/O mapping using the pgd/pmd entries already populated
 * in head.S as this function is called too early to allocated any memory. The
 * mapping size is 2MB with 4KB pages or 64KB or 64KB pages.
 */
void __iomem * __init early_io_map(phys_addr_t phys, unsigned long virt)
{
	unsigned long size, mask;
	bool page64k = IS_ENABLED(ARM64_64K_PAGES);
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	/*
	 * No early pte entries with !ARM64_64K_PAGES configuration, so using
	 * sections (pmd).
	 */
	size = page64k ? PAGE_SIZE : SECTION_SIZE;
	mask = ~(size - 1);

	pgd = pgd_offset_k(virt);
	pud = pud_offset(pgd, virt);
	if (pud_none(*pud))
		return NULL;
	pmd = pmd_offset(pud, virt);

	if (page64k) {
		if (pmd_none(*pmd))
			return NULL;
		pte = pte_offset_kernel(pmd, virt);
		set_pte(pte, __pte((phys & mask) | PROT_DEVICE_nGnRE));
	} else {
		set_pmd(pmd, __pmd((phys & mask) | PROT_SECT_DEVICE_nGnRE));
	}

	return (void __iomem *)((virt & mask) + (phys & ~mask));
}
#endif
#endif

static void __init map_mem(void)
{
    struct memblock_region *reg;
    phys_addr_t limit;

    /*
     * Temporarily limit the memblock range. We need to do this as
     * create_mapping requires puds, pmds and ptes to be allocated from
     * memory addressable from the initial direct kernel mapping.
     *
     * The initial direct kernel mapping, located at swapper_pg_dir,
     * gives us PGDIR_SIZE memory starting from PHYS_OFFSET (which must be
     * aligned to 2MB as per Documentation/arm64/booting.txt).
     */
    limit = PHYS_OFFSET + PGDIR_SIZE;
    memblock_set_current_limit(limit);

    /* map all the memory banks */
    for_each_memblock(memory, reg) {
        phys_addr_t start = reg->base;
        phys_addr_t end = start + reg->size;

        if (start >= end)
            break;

#ifndef CONFIG_ARM64_64K_PAGES
        /*
         * For the first memory bank align the start address and
         * current memblock limit to prevent create_mapping() from
         * allocating pte page tables from unmapped memory.
         * When 64K pages are enabled, the pte page table for the
         * first PGDIR_SIZE is already present in swapper_pg_dir.
         */
        if (start < limit)
            start = ALIGN(start, PMD_SIZE);
        if (end < limit) {
            limit = end & PMD_MASK;
            memblock_set_current_limit(limit);
        }
#endif

        create_mapping(start, __phys_to_virt(start), end - start);
    }

    /* Limit no longer required. */
    memblock_set_current_limit(MEMBLOCK_ALLOC_ANYWHERE);
}



/*
 * paging_init() sets up the page tables, initialises the zone memory
 * maps and sets up the zero page.
 */
void __init paging_init(void)
{
	phys_addr_t start, end;
	u64 i;
	struct memblock_region *reg;
	void *zero_page;

	//(xpte_t *) __va(PHYS_OFFSET + TEXT_OFFSET - SWAPPER_DIR_SIZE);
	// Setup user page tables for the kernel, zeroed out
	bootstrap_aspace.arch.upt = __va(PHYS_OFFSET + TEXT_OFFSET - SWAPPER_DIR_SIZE);//early_alloc(PAGE_SIZE);

	init_mem_pgprot(); // Not sure we need this for aspaces later on
	map_mem();

	/*
	 * Finally flush the caches and tlb to ensure that we're in a
	 * consistent state.
	 */
	flush_cache_all();
	flush_tlb_all();

	/* allocate the zero page. */
	zero_page = early_alloc(PAGE_SIZE);

	//bootmem_init();

	//empty_zero_page = virt_to_page(zero_page);
	//__flush_dcache_page(empty_zero_page);

	/*
	 * TTBR0 is only used for the identity mapping at this stage. Make it
	 * point to zero page to avoid speculatively fetching new entries.
	 */
	//cpu_set_reserved_ttbr0();
	//flush_tlb_all();
}
#if 0

/*
 * Enable the identity mapping to allow the MMU disabling.
 */
void setup_mm_for_reboot(void)
{
	cpu_switch_mm(idmap_pg_dir, &init_mm);
	flush_tlb_all();
}

/*
 * Check whether a kernel address is valid (derived from arch/x86/).
 */
int kern_addr_valid(unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	if ((((long)addr) >> VA_BITS) != -1UL)
		return 0;

	pgd = pgd_offset_k(addr);
	if (pgd_none(*pgd))
		return 0;

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud))
		return 0;

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		return 0;

	pte = pte_offset_kernel(pmd, addr);
	if (pte_none(*pte))
		return 0;

	return pfn_valid(pte_pfn(*pte));
}
#ifdef CONFIG_SPARSEMEM_VMEMMAP
#ifdef CONFIG_ARM64_64K_PAGES
int __meminit vmemmap_populate(struct page *start_page,
			       unsigned long size, int node)
{
	return vmemmap_populate_basepages(start_page, size, node);
}
#else	/* !CONFIG_ARM64_64K_PAGES */
int __meminit vmemmap_populate(struct page *start_page,
			       unsigned long size, int node)
{
	unsigned long addr = (unsigned long)start_page;
	unsigned long end = (unsigned long)(start_page + size);
	unsigned long next;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;

	do {
		next = pmd_addr_end(addr, end);

		pgd = vmemmap_pgd_populate(addr, node);
		if (!pgd)
			return -ENOMEM;

		pud = vmemmap_pud_populate(pgd, addr, node);
		if (!pud)
			return -ENOMEM;

		pmd = pmd_offset(pud, addr);
		if (pmd_none(*pmd)) {
			void *p = NULL;

			p = vmemmap_alloc_block_buf(PMD_SIZE, node);
			if (!p)
				return -ENOMEM;

			set_pmd(pmd, __pmd(__pa(p) | prot_sect_kernel));
		} else
			vmemmap_verify((pte_t *)pmd, node, addr, next);
	} while (addr = next, addr != end);

	return 0;
}

#endif	/* CONFIG_ARM64_64K_PAGES */
#endif	/* CONFIG_SPARSEMEM_VMEMMAP */
#endif
