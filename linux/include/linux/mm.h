#ifndef __LINUX_MM_H
#define __LINUX_MM_H

#include <linux/gfp.h>
#include <linux/rbtree.h>
#include <arch/pgtable.h>

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	ALIGN(addr, PAGE_SIZE)

/* finds the offset of a buffer in the page it starts in */
#define offset_in_page(p)	((unsigned long)(p) & ~PAGE_MASK)

#define mm_struct aspace
extern void mmput(struct mm_struct *);

struct page {
	void *virtual;
	int order;
	int user;
};

struct vm_operations_struct {
	void (*open)(struct vm_area_struct *area);
	void (*close)(struct vm_area_struct *area);

	/* called by access_process_vm when get_user_pages() fails, typically
	 * for use by special VMAs that can switch between memory and hardware */
	int (*access)(struct vm_area_struct *vma, unsigned long addr, void *buf, int len, int write);
};

struct vm_area_struct {
	unsigned long vm_start;         /* Our start address within vm_mm. */
	unsigned long vm_end;           /* The first byte after our end address
                                           within vm_mm. */
	pgprot_t vm_page_prot;          /* Access permissions of this VMA. */

	unsigned long vm_pgoff;		/* Offset (within vm_file) in PAGE_SIZE
					   units, *not* PAGE_CACHE_SIZE */

	struct vm_operations_struct *vm_ops;
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

extern int
generic_access_phys(struct vm_area_struct *vma, unsigned long addr,
                    void *buf, int len, int write);

#endif
