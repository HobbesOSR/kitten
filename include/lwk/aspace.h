/* Copyright (c) 2007, Sandia National Laboratories */

#ifndef _LWK_ASPACE_H
#define _LWK_ASPACE_H

#include <lwk/list.h>
#include <lwk/spinlock.h>
#include <arch/aspace.h>

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
aspace_kmem_alloc_region(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	extent,
	unsigned long	flags,
	unsigned long	pagesz,
	const char *	name,
	void **		kmem
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
