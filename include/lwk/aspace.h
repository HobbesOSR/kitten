/* Copyright (c) 2007,2008 Sandia National Laboratories */
/** \file
 *
 * Address space data structure.
 *
 */

#ifndef _LWK_ASPACE_H
#define _LWK_ASPACE_H

#include <lwk/types.h>
#include <lwk/idspace.h>
#include <lwk/futex.h>
#include <arch/aspace.h>

/** \name User-space address space IDs
 * Valid user-space address space IDs are in interval
 * [UASPACE_MIN_ID, UASPACE_MAX_ID].
 *
 * \note This interval must not include KERNEL_ASPACE_ID.
 * @{
 */
#define ASPACE_ID_USER_BIT      15  /* support 2^15 user-space aspaces */
#define UASPACE_MIN_ID          (1 << ASPACE_ID_USER_BIT)
#define UASPACE_MAX_ID          UASPACE_MIN_ID + (1 << ASPACE_ID_USER_BIT) - 1

/**
 * ID of the address space used by the init_task, the first user-level task.
 * Put it at the top of the user-space ID interval to keep it out of the way.
 */
#define INIT_ASPACE_ID          UASPACE_MAX_ID
//@}

/** \name Virtual memory flags.
 *
 * Protection and memory type flags.
 *
 * @{
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

// @}

/** \name Page sizes.
 * @{
 */
#define VM_PAGE_4KB       (1 << 12)
#define VM_PAGE_2MB       (1 << 21)
#define VM_PAGE_1GB       (1 << 30)
typedef unsigned long vmpagesize_t;
// @}

/** \name Core address space management API.
 *
 * These are accessible from both kernel-space and user-space (via syscalls).
 * @{
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

extern int aspace_virt_to_phys(id_t id, vaddr_t vaddr, paddr_t *paddr);

extern int aspace_dump2console(id_t id);
// @}

/** \name Convenience functions defined in liblwk.
 * @{
 */
extern int
aspace_map_region(
	id_t         id,
	vaddr_t      start,
	size_t       extent,
	vmflags_t    flags,
	vmpagesize_t pagesz,
	const char * name,
	paddr_t      pmem
);

extern int
aspace_map_region_anywhere(
	id_t         id,
	vaddr_t *    start,
	size_t       extent,
	vmflags_t    flags,
	vmpagesize_t pagesz,
	const char * name,
	paddr_t      pmem
);

// @}

#ifdef __KERNEL__

#include <lwk/spinlock.h>
#include <lwk/list.h>
#include <lwk/init.h>
#include <arch/aspace.h>

/**
 * Address space structure.
 *
 * This structure represents the kernel's view of an address space,
 * either user or kernel space.  The address space consists of
 * non-overlapping regions, stored in the region_list member.
 * The struct region is opaque to users of the high-level API.
 *
 */
struct aspace {
	spinlock_t         lock;        /**< Must be held to access addr space */

	id_t               id;          /**< The address space's ID */
	char               name[16];    /**< Address space's name */
	struct hlist_node  ht_link;     /**< Adress space hash table linkage */
	int                refcnt;      /**< # of users of this address space */

	struct list_head   region_list; /**< Sorted non-overlapping region list */

	/** \name Heap extents and sub-heap regions.
	 *
	 * The address space's "Heap" region is special in that it is
	 * subdivided into two separated regions.  The entire address
	 * space spans from:
	 *     [heap_start, heap_end)
	 *
	 * The traditional UNIX data segment is contained in the
	 * lower part of the address space's heap region, ranging from:
	 *     [heap_start, brk)
	 *
	 * Memory for anonymous mmap() regions is allocated from the top
	 * of the address space's heap region, ranging from:
	 *     [mmap_brk, heap_end)
	 *
	 * @{
	 */
	vaddr_t            heap_start;
	vaddr_t            heap_end;

	/** \note
	 * GLIBC/malloc will call the sys_brk() system call when it wants to
	 * expand or shrink the data segment. The kernel verifies that the new
	 * brk value is legal before updating it. The data segment may not
	 * extend beyond the address space's heap region or overlap with
	 * any anonymous mmap regions (see mmap_brk below).
	 */
	vaddr_t            brk;

	/** \note
	 * GLIBC makes at least one mmap() call during pre-main app startup
	 * to allocate some "anonymous" memory (i.e., normal memory, not a
	 * file mapping). mmap_brk starts out set to heap_end and grows down
	 * as anonymous mmap() calls are made. The kernel takes care to prevent
	 * mmap_brk from extending into the UNIX data segment (see brk above).
	 */
	vaddr_t            mmap_brk;
	// @}

	/**
 	 * Address space private futexes.
 	 */
	struct futex_queue futex_queues[1<<FUTEX_HASHBITS];

	/**
	 * Architecture specific address space data.
	 */
	struct arch_aspace arch;
};

/**
 * ID of the address space used by kernel tasks.
 */
#define KERNEL_ASPACE_ID        0

/** \name Kernel-only address space API.
 *
 * Kernel-only unlocked versions of the core adress space management API.
 * These assume that the aspace objects passed in have already been locked.
 * The caller must unlock the aspaces. The caller must also ensure that
 * interrupts are disabled before calling these functions.
 * @{
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
extern int __aspace_virt_to_phys(struct aspace *aspace,
                                 vaddr_t vaddr, paddr_t *paddr);
// @}


/** \name Kernel-only address space management API.
 *
 * \note These are not accessible from user-space.
 */
extern int __init aspace_subsys_init(void);
extern struct aspace *aspace_acquire(id_t id);
extern void aspace_release(struct aspace *aspace);
// @}

/** \name Architecture specific address space functions.
 *
 * \note Each architecture port must provide these.
 * @{
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
extern int arch_aspace_virt_to_phys(struct aspace *aspace,
                                    vaddr_t vaddr, paddr_t *paddr);

// @}

/** \name System call handlers.
 *
 * These are system call handler wrappers for the address space API.
 *
 * @{
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
extern int sys_aspace_virt_to_phys(id_t id,
                                   vaddr_t vaddr, paddr_t __user *paddr);
extern int sys_aspace_dump2console(id_t id);

//@}

#endif // __KERNEL__
#endif
