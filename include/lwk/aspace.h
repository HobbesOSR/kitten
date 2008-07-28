/* Copyright (c) 2007, Sandia National Laboratories */

#ifndef _LWK_ASPACE_H
#define _LWK_ASPACE_H

#include <lwk/list.h>
#include <lwk/spinlock.h>
#include <arch/aspace.h>
#include <arch/mman.h>

/**
 * Protection and memory type flags.
 */
#define VM_READ		(1 << 0)
#define VM_WRITE	(1 << 1)
#define VM_EXEC		(1 << 2)
#define VM_NOCACHE	(1 << 3)
#define VM_WRITETHRU	(1 << 4)
#define VM_USER		(1 << 5)
#define VM_GLOBAL	(1 << 6)

/**
 * Page sizes.
 */
#define VM_PAGE_4KB	(1 << 12)
#define VM_PAGE_2MB	(1 << 21)
#define VM_PAGE_1GB	(1 << 30)

/**
 * Page sizes supported by the architecture... above flags OR'ed together.
 */
extern unsigned long supported_pagesz_mask;

/**
 * Address space structure.
 */
struct aspace {
	spinlock_t         lock;        /* Must be held to modify addr space */

	int                refcnt;      /* # tasks using this address space */
	struct list_head   region_list; /* Sorted non-overlapping region list */

	/**
	 * The address space's "Heap" region spans from:
	 *     [heap_start, heap_end)
	 */
	unsigned long      heap_start;
	unsigned long      heap_end;

	/**
	 * The traditional UNIX data segment is contained in the address
	 * space's heap region, ranging from:
	 *     [heap_start, brk)
	 *
	 * GLIBC/malloc will call the sys_brk() system call when it wants to
	 * expand or shrink the data segment. The kernel verifies that the new
	 * brk value is legal before updating it. The data segment may not
	 * extend beyond the address space's heap region or overlap with
	 * any anonymous mmap regions (see mmap_brk below).
	 */
	unsigned long brk;

	/**
	 * Memory for anonymous mmap() regions is allocated from the top of the
	 * address space's heap region, ranging from:
	 *     [mmap_brk, heap_end)
	 *
	 * GLIBC makes at least one mmap() call during pre-main app startup
	 * to allocate some "anonymous" memory (i.e., normal memory, not a
	 * file mapping). mmap_brk starts out set to heap_end and grows down
	 * as anonymous mmap() calls are made. The kernel takes care to prevent
	 * mmap_brk from extending into the UNIX data segment (see brk above).
	 */
	unsigned long mmap_brk;

	/**
 	 * ELF load information
 	 */
	void __user * entry_point;  /* user address of first instruction */
	void __user * stack_ptr;    /* user address of initial stack ptr */
	void __user * e_phdr;       /* user address of ELF program header */
	unsigned long e_phnum;      /* number of ELF program headers */

	struct arch_aspace arch;        /* Architecture specific data */
};

extern struct aspace *aspace_create(void);
extern void aspace_destroy(struct aspace *aspace);

extern void aspace_activate(struct aspace * aspace);

extern int aspace_aquire(struct aspace *aspace);
extern int aspace_release(struct aspace *aspace);

extern int
aspace_add_region(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	extent,
	unsigned long	flags,
	unsigned long	pagesz,
	const char *	name
);

extern int
aspace_del_region(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	extent
);

extern int
aspace_map_memory(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	paddr,
	unsigned long	extent
);

extern int
aspace_unmap_memory(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	extent
);

extern int
aspace_alloc_region(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	extent,
	unsigned long	flags,
	unsigned long	pagesz,
	const char *	name,
	void * (*alloc_mem)(size_t size, size_t alignment),
	void **		mem
);

extern void aspace_dump(struct aspace *aspace);

extern int arch_aspace_create(struct aspace *aspace);
extern void arch_aspace_destroy(struct aspace *aspace);

extern void arch_aspace_activate(struct aspace * aspace);

extern int
arch_aspace_map_page(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	paddr,
	unsigned long	flags,
	unsigned long	pagesz
);

extern void
arch_aspace_unmap_page(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	pagesz
);

#endif
