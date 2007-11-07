#ifndef _LWK_MM_H
#define _LWK_MM_H

/**
 * Virtual memory area structure.
 */
struct vma_struct {
	struct mm_struct  *mm;    /* The address space we belong to */

	unsigned long     start;  /* Starting addr in the addr space */
	unsigned long     end;    /* 1st byte after the VMA's ending addr */
	uint32_t          flags;  /* Flags that qualify the VMA */
	uint32_t          pagesz; /* Valid page sizes... 2^bit */

	struct vma_struct *next;  /* Per-task VMA list, sorted by addr */
};

/**
 * vma_struct.flags flags
 */
#define VM_READ		(1 << 0)
#define VM_WRITE	(1 << 1)
#define VM_EXEC		(1 << 2)
#define VM_NOCACHE	(1 << 3)
#define VM_WRITETHRU	(1 << 4)
#define VM_USER		(1 << 5)
#define VM_GLOBAL	(1 << 6)

/**
 * vma_struct.pagesz bit names
 */
#define VM_PAGE_4KB	(1 << 12)
#define VM_PAGE_2MB	(1 << 21)
#define VM_PAGE_1GB	(1 << 30)

extern int vma_insert(struct mm_struct *mm, struct vma_struct *vma);

extern int vma_bind_mem( struct vma_struct *vma,
                         unsigned long     phys_start,
                         unsigned long     virt_start,
                         unsigned long     extent );

extern int vma_unbind_mem( struct vma_struct *vma,
                           unsigned long     virt_start,
                           unsigned long     extent );

#endif
