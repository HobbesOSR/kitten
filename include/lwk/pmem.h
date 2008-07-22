/* Copyright (c) 2008, Sandia National Laboratories */

#ifndef _LWK_PMEM_H
#define _LWK_PMEM_H

/**
 * Physical memory types.
 */
typedef enum {
	PMEM_TYPE_BOOTMEM     = 0,  /* memory allocated at boot-time */
	PMEM_TYPE_KMEM        = 1,  /* memory managed by the kernel */
	PMEM_TYPE_UMEM        = 2,  /* memory managed by user-space */
} pmem_type_t;

/**
 * Defines a physical memory region.
 */
struct pmem_region {
	unsigned long   start;             /* region occupies: [start, end) */
	unsigned long   end;

	bool            type_is_set;       /* type field is set? */
	pmem_type_t     type;              /* physical memory type */

	bool            lgroup_is_set;     /* lgroup field is set? */
	lgroup_t        lgroup;            /* locality group region is in */

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

#ifdef __KERNEL__

/**
 * System call handlers.
 */
int sys_pmem_add(const struct pmem_region __user * rgn);
int sys_pmem_update(const struct pmem_region __user * update);
int sys_pmem_query(const struct pmem_region __user * query,
                   struct pmem_region __user * result);

#endif
#endif
