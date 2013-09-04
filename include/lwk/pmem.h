/* Copyright (c) 2008, Sandia National Laboratories */

#ifndef _LWK_PMEM_H
#define _LWK_PMEM_H

#include <lwk/types.h>

/**
 * Physical memory types.
 */
typedef enum {
	PMEM_TYPE_BOOTMEM     = 0,  /* memory allocated at boot-time */
	PMEM_TYPE_BIGPHYSAREA = 1,  /* memory set-aside for a device/driver */
	PMEM_TYPE_INITRD      = 2,  /* initrd image provided by bootloader */
	PMEM_TYPE_INIT_TASK   = 3,  /* memory used by the initial task */
	PMEM_TYPE_KMEM        = 4,  /* memory managed by the kernel */
	PMEM_TYPE_UMEM        = 5,  /* memory managed by user-space */
} pmem_type_t;

/**
 * Defines a physical memory region.
 */
struct pmem_region {
	paddr_t         start;             /* region occupies: [start, end) */
	paddr_t         end;

	bool            type_is_set;       /* type field is set? */
	pmem_type_t     type;              /* physical memory type */

	bool            numa_node_is_set;  /* numa_node field is set? */
	numa_node_t     numa_node;         /* locality group region is in */

	bool            allocated_is_set;  /* allocated field set? */
	bool            allocated;         /* region is allocated? */ 

	bool            name_is_set;       /* name field is set? */
	char            name[32];          /* human-readable name of region */

};

/**
 * Core physical memory management functions.
 */
int pmem_add(const struct pmem_region *rgn);
int pmem_update(const struct pmem_region *update);
int pmem_query(const struct pmem_region *query, struct pmem_region *result);
int pmem_alloc(size_t size, size_t alignment,
               const struct pmem_region *constraint,
               struct pmem_region *result);
int pmem_zero(const struct pmem_region *rgn);

/**
 * Convenience functions.
 */
void pmem_region_unset_all(struct pmem_region *rgn);
const char *pmem_type_to_string(pmem_type_t type);
int pmem_alloc_umem(size_t size, size_t alignment, struct pmem_region *rgn);
bool pmem_is_type(pmem_type_t type, paddr_t start, size_t extent);
void pmem_dump2console(void);

#ifdef __KERNEL__

/**
 * System call handlers.
 */
int sys_pmem_add(const struct pmem_region __user * rgn);
int sys_pmem_update(const struct pmem_region __user * update);
int sys_pmem_query(const struct pmem_region __user * query,
                   struct pmem_region __user * result);
int sys_pmem_alloc(size_t size, size_t alignment,
                   const struct pmem_region __user *constraint,
                   struct pmem_region __user *result);
int sys_pmem_zero(const struct pmem_region __user *rgn);

#endif
#endif
