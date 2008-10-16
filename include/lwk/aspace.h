/* Copyright (c) 2007,2008 Sandia National Laboratories */

#ifndef _LWK_ASPACE_H
#define _LWK_ASPACE_H

#include <lwk/types.h>
#include <lwk/idspace.h>
#include <arch/aspace.h>

/**
 * Valid user-space created address space IDs are in interval
 * [ASPACE_MIN_ID, ASPACE_MAX_ID].
 */
#define ASPACE_MIN_ID     0
#define ASPACE_MAX_ID     4094

/**
 * The address space ID to use for the init_task.
 * Put it at the top of the space to keep it out of the way.
 */
#define INIT_ASPACE_ID    ASPACE_MAX_ID

/**
 * Protection and memory type flags.
 */
#define VM_READ           (1 << 0)
#define VM_WRITE          (1 << 1)
#define VM_EXEC           (1 << 2)
#define VM_NOCACHE        (1 << 3)
#define VM_WRITETHRU      (1 << 4)
#define VM_GLOBAL         (1 << 5)
#define VM_USER           (1 << 6)
#define VM_KERNEL         (1 << 7)
#define VM_HEAP           (1 << 8)
#define VM_SMARTMAP       (1 << 9)
typedef unsigned long vmflags_t;

/**
 * Page sizes.
 */
#define VM_PAGE_4KB       (1 << 12)
#define VM_PAGE_2MB       (1 << 21)
#define VM_PAGE_1GB       (1 << 30)
typedef unsigned long vmpagesize_t;

/**
 * Core address space management API.
 * These are accessible from both kernel-space and user-space (via syscalls).
 */
extern int aspace_get_myid(id_t *id);
extern int aspace_create(id_t id_request, const char *name, id_t *id);
extern int aspace_destroy(id_t id);

extern int aspace_find_hole(id_t id,
                            vaddr_t start_hint, size_t extent, size_t alignment,
                            vaddr_t *start);

extern int aspace_add_region(id_t id,
                             vaddr_t start, size_t extent,
                             vmflags_t flags, vmpagesize_t pagesz,
                             const char *name);
extern int aspace_del_region(id_t id, vaddr_t start, size_t extent);

extern int aspace_map_pmem(id_t id,
                           paddr_t pmem, vaddr_t start, size_t extent);
extern int aspace_unmap_pmem(id_t id, vaddr_t start, size_t extent);

extern int aspace_smartmap(id_t src, id_t dst, vaddr_t start, size_t extent);
extern int aspace_unsmartmap(id_t src, id_t dst);

extern int aspace_dump2console(id_t id);

/**
 * Convenience functions defined in liblwk.
 */
extern int aspace_map_region(id_t id,
                             vaddr_t start, size_t extent,
                             vmflags_t flags, vmpagesize_t pagesz,
                             const char *name, paddr_t pmem);

#ifdef __KERNEL__

#include <lwk/spinlock.h>
#include <lwk/list.h>
#include <lwk/init.h>
#include <arch/aspace.h>

/**
 * Address space structure.
 */
struct aspace {
	spinlock_t         lock;        /* Must be held to access addr space */

	id_t               id;          /* The address space's ID */
	char               name[16];    /* Address space's name */
	struct hlist_node  ht_link;     /* Adress space hash table linkage */
	int                refcnt;      /* # of users of this address space */

	struct list_head   region_list; /* Sorted non-overlapping region list */

	/**
	 * The address space's "Heap" region spans from:
	 *     [heap_start, heap_end)
	 */
	vaddr_t            heap_start;
	vaddr_t            heap_end;

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
	vaddr_t            brk;

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
	vaddr_t            mmap_brk;

	/**
	 * Architecture specific address space data.
	 */
	struct arch_aspace arch;
};

/**
 * Valid address space IDs are in interval [__ASPACE_MIN_ID, __ASPACE_MAX_ID].
 */
#define __ASPACE_MIN_ID   ASPACE_MIN_ID
#define __ASPACE_MAX_ID   ASPACE_MAX_ID+1  /* +1 for KERNEL_ASPACE_ID */

/**
 * ID of the address space used by kernel threads.
 */
#define KERNEL_ASPACE_ID  ASPACE_MAX_ID+1

/**
 * Kernel-only unlocked versions of the core adress space management API.
 * These assume that the aspace objects passed in have already been locked.
 * The caller must unlock the aspaces. The caller must also ensure that
 * interrupts are disabled before calling these functions.
 */
extern int __aspace_find_hole(struct aspace *aspace,
                              vaddr_t start_hint, size_t extent, size_t alignment,
                              vaddr_t *start);
extern int __aspace_add_region(struct aspace *aspace,
                               vaddr_t start, size_t extent,
                               vmflags_t flags, vmpagesize_t pagesz,
                               const char *name);
extern int __aspace_del_region(struct aspace *aspace,
                               vaddr_t start, size_t extent);
extern int __aspace_map_pmem(struct aspace *aspace,
                             paddr_t pmem, vaddr_t start, size_t extent);
extern int __aspace_unmap_pmem(struct aspace *aspace,
                               vaddr_t start, size_t extent);
extern int __aspace_smartmap(struct aspace *src, struct aspace *dst,
                             vaddr_t start, size_t extent);
extern int __aspace_unsmartmap(struct aspace *src, struct aspace *dst);

/**
 * Kernel-only address space management API.
 * These are not accessible from user-space.
 */
extern int __init aspace_subsys_init(void);
extern struct aspace *aspace_acquire(id_t id);
extern void aspace_release(struct aspace *aspace);

/**
 * Architecture specific address space functions.
 * Each architecture port must provide these.
 */
extern int arch_aspace_create(struct aspace *aspace);
extern void arch_aspace_destroy(struct aspace *aspace);
extern void arch_aspace_activate(struct aspace *aspace);
extern int arch_aspace_map_page(struct aspace * aspace,
                                vaddr_t start, paddr_t paddr,
                                vmflags_t flags, vmpagesize_t pagesz);
extern void arch_aspace_unmap_page(struct aspace * aspace,
                                   vaddr_t start, vmpagesize_t pagesz);
extern int arch_aspace_smartmap(struct aspace *src, struct aspace *dst,
                                vaddr_t start, size_t extent);
extern int arch_aspace_unsmartmap(struct aspace *src, struct aspace *dst,
                                  vaddr_t start, size_t extent);

/**
 * System call handlers.
 */
extern int sys_aspace_get_myid(id_t __user *id);
extern int sys_aspace_create(id_t id_request, const char __user *name,
                             id_t __user *id);
extern int sys_aspace_destroy(id_t id);
extern int sys_aspace_find_hole(id_t id, vaddr_t start_hint, size_t extent,
                                size_t alignment, vaddr_t __user *start);
extern int sys_aspace_add_region(id_t id,
                                 vaddr_t start, size_t extent,
                                 vmflags_t flags, vmpagesize_t pagesz,
                                 const char __user *name);
extern int sys_aspace_del_region(id_t id, vaddr_t start, size_t extent);
extern int sys_aspace_map_pmem(id_t id,
                               paddr_t pmem, vaddr_t start, size_t extent);
extern int sys_aspace_unmap_pmem(id_t id, vaddr_t start, size_t extent);
extern int sys_aspace_smartmap(id_t src, id_t dst,
                               vaddr_t start, size_t extent);
extern int sys_aspace_unsmartmap(id_t src, id_t dst);
extern int sys_aspace_dump2console(id_t id);

#endif
#endif
