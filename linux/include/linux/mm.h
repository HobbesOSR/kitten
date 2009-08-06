#ifndef __LINUX_MM_H
#define __LINUX_MM_H

#include <linux/gfp.h>
#include <linux/rbtree.h>
#include <arch/pgtable.h>

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)

struct page { };

struct vm_area_struct {
	unsigned long vm_start;         /* Our start address within vm_mm. */
	unsigned long vm_end;           /* The first byte after our end address
                                           within vm_mm. */
	pgprot_t vm_page_prot;          /* Access permissions of this VMA. */
};

extern void
put_page(struct page *page);

extern void
get_page(struct page *page);

extern int
set_page_dirty_lock(struct page *page);

extern int
can_do_mlock(void);

extern int
remap_pfn_range(struct vm_area_struct *, unsigned long addr,
                unsigned long pfn, unsigned long size, pgprot_t);

extern void *
lowmem_page_address(struct page *page);

extern int
get_user_pages(struct task_struct *tsk, struct mm_struct *mm,
               unsigned long start, int len, int write, int force,
               struct page **pages, struct vm_area_struct **vmas);

#endif
