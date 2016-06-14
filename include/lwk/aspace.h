/* Copyright (c) 2007-2010 Sandia National Laboratories */

#ifndef _LWK_ASPACE_H
#define _LWK_ASPACE_H

#include <lwk/types.h>
#include <lwk/idspace.h>
#include <lwk/futex.h>
#include <lwk/cpumask.h>
#include <arch/aspace.h>


// Valid user-space address space IDs are in interval
// [UASPACE_MIN_ID, UASPACE_MAX_ID]. This interval must
// not include KERNEL_ASPACE_ID.
#define UASPACE_MIN_ID		1
#define UASPACE_MAX_ID		32767


// IDs of well-known address spaces
#define KERNEL_ASPACE_ID	0
#define INIT_ASPACE_ID		UASPACE_MIN_ID


// Virtual memory protection and memory type flags
#define VM_READ			(1 << 0)
#define VM_WRITE		(1 << 1)
#define VM_EXEC			(1 << 2)
#define VM_NOCACHE		(1 << 3)
#define VM_WRITETHRU		(1 << 4)
#define VM_GLOBAL		(1 << 5)
#define VM_USER			(1 << 6)
#define VM_KERNEL		(1 << 7)
#define VM_HEAP			(1 << 8)
#define VM_SMARTMAP		(1 << 9)
typedef unsigned long vmflags_t;


// Symbolic names for various page sizes.
// Note that a given architecture will likely only support a subset of these.
#define VM_PAGE_4KB		(1 << 12)
#define VM_PAGE_2MB		(1 << 21)
#define VM_PAGE_1GB		(1 << 30)
typedef unsigned long vmpagesize_t;


typedef struct {
	vaddr_t     start;
	vaddr_t     end;
	paddr_t     paddr;
	vmflags_t   flags; 
} aspace_mapping_t;


// Begin core address space management API.
// These are accessible from both kernel-space and user-space (via syscalls).

extern int
aspace_get_myid(
	id_t *			id
);

extern int
aspace_create(
	id_t			id_request,
	const char *		name,
	id_t *			id
);

extern int
aspace_destroy(
	id_t			id
);

extern int
aspace_update_user_cpumask(
        id_t                    id,
	user_cpumask_t *        cpu_mask
);

extern int
aspace_find_hole(
	id_t			id,
	vaddr_t			start_hint,
	size_t			extent,
	size_t			alignment,
	vaddr_t *		start
);

extern int
aspace_add_region(
	id_t			id,
	vaddr_t			start,
	size_t			extent,
	vmflags_t		flags,
	vmpagesize_t		pagesz,
	const char *		name
);

extern int
aspace_del_region(
	id_t			id,
	vaddr_t			start,
	size_t			extent
);

extern int
aspace_map_pmem(
	id_t			id,
	paddr_t			pmem,
	vaddr_t			start,
	size_t			extent
);

extern int
aspace_unmap_pmem(
	id_t			id,
	vaddr_t			start,
	size_t			extent
);

extern int
aspace_smartmap(
	id_t			src,
	id_t			dst,
	vaddr_t			start,
	size_t			extent
);

extern int
aspace_unsmartmap(
	id_t			src,
	id_t			dst
);

extern int
aspace_virt_to_phys(
	id_t			id,
	vaddr_t			vaddr,
	paddr_t *		paddr
);

extern int
aspace_lookup_mapping(
        id_t                    id, 
	vaddr_t                 vaddr,
	aspace_mapping_t *      mapping
);

extern int aspace_dump2console(
	id_t			id
);

extern int
aspace_set_rank(id_t id,
                id_t rank
);

extern int
aspace_get_rank(
                id_t *id
);

// End core address space management API


// Begin convenience functions defined in liblwk

extern int
aspace_map_region(
	id_t			id,
	vaddr_t			start,
	size_t			extent,
	vmflags_t		flags,
	vmpagesize_t		pagesz,
	const char *		name,
	paddr_t			pmem
);

extern int
aspace_map_region_anywhere(
	id_t			id,
	vaddr_t *		start,
	size_t			extent,
	vmflags_t		flags,
	vmpagesize_t		pagesz,
	const char *		name,
	paddr_t			pmem
);

extern int
aspace_unmap_region(
	id_t                    id,
	vaddr_t                 start,
	size_t                  extent
);

// End convenience functions defined in liblwk


#ifdef __KERNEL__

#include <lwk/semaphore.h>
#include <lwk/spinlock.h>
#include <lwk/list.h>
#include <lwk/init.h>
#include <lwk/signal.h>
#include <lwk/waitq.h>
#include <arch/aspace.h>


// Address space structure
//
// This structure represents the kernel's view of an address space,
// either user or kernel space. The address space consists of
// non-overlapping regions, stored in the region_list member.
// The struct region is opaque to users of the high-level API.
struct aspace {
	spinlock_t		lock;		// Synchronizes access to aspace
	int			refcnt;		// Must be 0 to destroy aspace
	bool			exiting;	// If true, no new tasks allowed
	bool			reaped;		// If true, aspace has been waited on

	id_t			id;		// aspace's ID
	id_t			rank;	// aspace's rank
	char			name[32];	// aspace's name
	struct hlist_node	ht_link;	// Linkage for aspace hash table

	struct aspace *		parent;		// aspace that created this aspace
	struct list_head	child_list;	// list of aspaces this aspace has created
	struct list_head	child_link;	// linkage for child_list
	waitq_t			child_exit_waitq; // Wait queue for waiting on child exits

	struct list_head	region_list;	// Sorted non-overlapping region list

	struct list_head	task_list;	// List of tasks using this aspace
	id_t			next_task_id;	// ID for next task created in aspace

	cpumask_t		cpu_mask;	// CPUs this aspace is available on
	id_t			next_cpu_id;	// CPU ID for next task created in aspace

	int			exit_status;	// Value to return to waitpid() and friends

	// Heap extents and sub-heap regions.
	//
	// The address space's "Heap" region is special in that it is
	// subdivided into two separated regions. The entire heap
	// region spans from:
	//     [heap_start, heap_end)
	//
	// The traditional UNIX data segment is contained in the
	// lower part of the heap region, ranging from:
	//     [heap_start, brk)
	//
	// Memory for anonymous mmap() regions is allocated from the top
	// of the heap region, ranging from:
	//     [mmap_brk, heap_end)
	vaddr_t			heap_start;
	vaddr_t			heap_end;
	vaddr_t			brk;
	vaddr_t			mmap_brk;

	// Needed for IB support
	struct semaphore	mmap_sem;
	unsigned long		locked_vm;

 	// Address space private futexes
	struct futex_queue	futex_queues[1<<FUTEX_HASHBITS];

	// Signal handling information
	struct sigaction	sigaction[NUM_SIGNALS];
	struct sigpending	sigpending;

	// Architecture specific address space data.
	struct arch_aspace	arch;
};


// Begin kernel-only "unlocked" versions of the core aspace management API.
// 
// These assume that the aspace objects passed in have already been locked.
// The caller must unlock the aspaces. The caller must also ensure that
// interrupts are disabled before calling these functions.

extern int
__aspace_find_hole(
	struct aspace *		aspace,
	vaddr_t			start_hint,
	size_t			extent,
	size_t			alignment,
	vaddr_t *		start
);

extern int
__aspace_add_region(
	struct aspace *		aspace,
	vaddr_t			start,
	size_t			extent,
	vmflags_t		flags,
	vmpagesize_t		pagesz,
	const char *		name
);

extern int
__aspace_del_region(
	struct aspace *		aspace,
	vaddr_t			start,
	size_t			extent
);


extern int
__aspace_map_pmem(
	struct aspace *		aspace,
	paddr_t			pmem,
	vaddr_t			start,
	size_t			extent
);

extern int
__aspace_unmap_pmem(
	struct aspace *		aspace,
	vaddr_t			start,
	size_t			extent
);

extern int
__aspace_smartmap(
	struct aspace *		src,
	struct aspace *		dst,
	vaddr_t			start,
	size_t			extent
);

extern int
__aspace_unsmartmap(
	struct aspace *		src,
	struct aspace *		dst
);

extern int
__aspace_virt_to_phys(
	struct aspace *		aspace,
	vaddr_t			vaddr,
	paddr_t *		paddr
);

// End kernel-only "unlocked" versions of the core aspace management API


// Begin kernel-only address space management API

extern int
__init aspace_subsys_init(
	void
);

extern int
aspace_update_cpumask(
        id_t               id,
	cpumask_t *        cpu_mask
);




extern struct aspace *
aspace_acquire(
	id_t			id
);

extern void
aspace_release(
	struct aspace *		aspace
);

extern int
aspace_wait4_child_exit(
	id_t			child_id,
	bool			block,
	id_t *			exit_id,
	int *			exit_status
);

// End kernel-only address space management API


// Begin architecture specific address space functions

extern int
arch_aspace_create(
	struct aspace *		aspace
);

extern void
arch_aspace_destroy(
	struct aspace *		aspace
);

extern void
arch_aspace_activate(
	struct aspace *		aspace
);

extern int
arch_aspace_map_page(
	struct aspace *		aspace,
	vaddr_t			start,
	paddr_t			paddr,
	vmflags_t		flags,
	vmpagesize_t		pagesz
);

extern void
arch_aspace_unmap_page(
	struct aspace *		aspace,
	vaddr_t			start,
	vmpagesize_t		pagesz
);

extern int
arch_aspace_smartmap(
	struct aspace *		src,
	struct aspace *		dst,
	vaddr_t			start,
	size_t			extent
);

extern int
arch_aspace_unsmartmap(
	struct aspace *		src,
	struct aspace *		dst,
	vaddr_t			start,
	size_t			extent
);

extern int
arch_aspace_virt_to_phys(
	struct aspace *		aspace,
	vaddr_t			vaddr,
	paddr_t *		paddr
);

extern int
arch_aspace_map_pmem_into_kernel(
	paddr_t			start,
	paddr_t			end
);

// End architecture specific address space functions


// Begin aspace system call handler prototypes

extern int
sys_aspace_get_myid(
	id_t __user *		id
);

extern int
sys_aspace_create(
	id_t			id_request,
	const char __user *	name,
	id_t __user *		id
);

extern int
sys_aspace_destroy(
	id_t			id
);

extern int
sys_aspace_update_user_cpumask(
        id_t                      id,
	user_cpumask_t __user *   cpu_mask
);


extern int
sys_aspace_find_hole(
	id_t			id,
	vaddr_t			start_hint,
	size_t			extent,
	size_t			alignment,
	vaddr_t __user *	start
);

extern int
sys_aspace_add_region(
	id_t			id,
	vaddr_t			start,
	size_t			extent,
	vmflags_t		flags,
	vmpagesize_t		pagesz,
	const char __user *	name
);

extern int
sys_aspace_del_region(
	id_t			id,
	vaddr_t			start,
	size_t			extent
);

extern int
sys_aspace_map_pmem(
	id_t			id,
	paddr_t			pmem,
	vaddr_t			start,
	size_t			extent
);

extern int
sys_aspace_unmap_pmem(
	id_t			id,
	vaddr_t			start,
	size_t			extent
);

extern int
sys_aspace_smartmap(
	id_t			src,
	id_t			dst,
	vaddr_t			start,
	size_t			extent
);

extern int
sys_aspace_unsmartmap(
	id_t			src,
	id_t			dst
);

extern int
sys_aspace_virt_to_phys(
	id_t			id,
	vaddr_t			vaddr,
	paddr_t __user *	paddr
);

extern int
sys_aspace_dump2console(
	id_t			id
);

extern int
sys_get_rank(
	id_t *id
);

extern int
sys_set_rank(
	id_t aspace_id,
	id_t rank
);
// End aspace system call handler prototypes


#endif // __KERNEL__
#endif
