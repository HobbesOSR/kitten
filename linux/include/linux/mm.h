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

#define VM_SHARED       0x00000008

#define VM_MAYREAD      0x00000010
#define VM_MAYWRITE     0x00000020
#define VM_MAYEXEC      0x00000040
#define VM_MAYSHARE     0x00000080

#define VM_DONTCOPY     0x00020000      /* Do not copy this vma on fork */
#define VM_DONTEXPAND   0x00040000      /* Cannot expand with mremap() */

#define VM_FAULT_SIGBUS 0x0002

#define VM_RESERVED     0x00080000

struct page {
	void *virtual;
	int order;
	int user;
};

#ifndef pgoff_t
#define pgoff_t unsigned long
#endif

struct vm_fault {
        unsigned int flags;             /* FAULT_FLAG_xxx flags */
        pgoff_t pgoff;                  /* Logical page offset based on vma */
        void __user *virtual_address;   /* Faulting virtual address */

        struct page *page;              /* ->fault handlers should return a
                                         * page here, unless VM_FAULT_NOPAGE
                                         * is set (which is also implied by
                                         * VM_FAULT_ERROR).
                                         */
};

struct vm_operations_struct {
	void (*open)(struct vm_area_struct *area);
	void (*close)(struct vm_area_struct *area);
	int (*fault)(struct vm_area_struct *vma, struct vm_fault *vmf);

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

	unsigned long vm_flags;
	void * vm_private_data;
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
