/* Copyright (c) 2007, Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/aspace.h>
#include <lwk/log2.h>


/**
 * Mask specifying which page sizes are supported. This needs to be set by the
 * architecture's bootstrap code. Each set bit represents that a page size of
 * 2^bit is supported.
 */
unsigned long supported_pagesz_mask;


/**
 * Memory region structure. A memory region represents a contiguous region 
 * [start, end) of valid memory addresses in an address space.
 */
struct region {
	struct aspace *  aspace;   /* Address space this region belongs to */
	struct list_head link;     /* Linkage in the aspace->region_list */

	unsigned long    start;    /* Starting address of the region */
	unsigned long    end;      /* 1st byte after end of the region */
	unsigned long    flags;    /* Permissions, caching, etc. */
	unsigned long    pagesz;   /* Allowed page sizes... 2^bit */

	char             name[16]; /* Human-readable name of the region */
};


/**
 * This calculates a region's end address. Normally end is the address of the
 * first byte after the region. However if the region extends to the end of
 * memory, that is not possible so set end to the last valid address,
 * ULONG_MAX.
 */
static unsigned long
calc_end(unsigned long start, unsigned long extent)
{
	unsigned long end = start + extent;
	if (end == 0)
		end = ULONG_MAX;
	return end;
}


/**
 * Locates the region covering the specified address.
 */
static struct region *
find_region(
	struct aspace *	aspace,
	unsigned long	addr
)
{
	struct region *rgn;
	
	list_for_each_entry(rgn, &aspace->region_list, link) {
		if ((rgn->start <= addr) && (rgn->end > addr))
			return rgn;
	}
	return NULL;
}


/**
 * Allocates and initializes a new address space object. The aspace returned
 * maps all kernel memory and may be switched to immediately. No user regions
 * are mapped in the returned aspace -- the range [0, PAGE_OFFSET) is
 * completely empty.
 *
 * Returns:
 *       Success: Pointer to a new address space.
 *       Failure: NULL
 */
struct aspace *
aspace_create(void)
{
	struct aspace *aspace;
	int status;

	if ((aspace = kmem_alloc(sizeof(struct aspace))) == NULL)
		return NULL;

	/*
	 * Initialize the address space. kmem_alloc() allocates zeroed memory
	 * so fields with an initial state of zero do not need to be explicitly
	 * initialized.
	 */
	INIT_LIST_HEAD(&aspace->region_list);
	aspace->refcnt = 1;

	/* Perform architecture specific address space initialization */
	status = arch_aspace_create(aspace);
	if (status) {
		kmem_free(aspace);
		return NULL;
	}

	return aspace;
}


/**
 * Destroys an address space. This frees all memory used by the aspace.
 */
void
aspace_destroy(struct aspace *aspace)
{
	struct list_head *pos;
	struct list_head *tmp;
	struct region *rgn;

	BUG_ON(aspace->refcnt);

	/* Free all of the address space's regions */
	list_for_each_safe(pos, tmp, &aspace->region_list) {
		rgn = list_entry(pos, struct region, link);
		list_del(&rgn->link);
		kmem_free(rgn);
	}

	/* Perform architecture specific address space destruction */
	arch_aspace_destroy(aspace);

	/* Free the address space object */
	kmem_free(aspace);
}


/**
 * Activates the address space on the calling CPU. After aspace_activate()
 * returns, the CPU is executing in the address space.
 */
void
aspace_activate(
	struct aspace *	aspace
)
{
	arch_aspace_activate(aspace);
}


/**
 * Increments an address space's reference count. Returns the new value.
 */
int
aspace_aquire(struct aspace *aspace)
{
	return ++aspace->refcnt;
}


/**
 * Decrements an address space's reference count. Returns the new value.
 */
int
aspace_release(struct aspace *aspace)
{
	--aspace->refcnt;
	BUG_ON(aspace->refcnt < 0);
	return aspace->refcnt;
}


/**
 * Adds a new region to an address space. The region must not overlap with any
 * existing regions. The region must also be aligned to the page size specified
 * and have an extent that is a multiple of the page size.
 *
 * Arguments:
 *       [IN] aspace: Address space to add the region to.
 *       [IN] start:  Starting address of region, must be pagesz aligned.
 *       [IN] extent: Size of the region in bytes, must be multiple of pagesz.
 *       [IN] flags:  Permissions, memory type, etc. (e.g., VM_WRITE | VM_READ).
 *       [IN] pagesz: Page size that should be used to map the region.
 *       [IN] name:   Human readable name of the region.
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, aspace is unmodified.
 */
int
aspace_add_region(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	extent,
	unsigned long	flags,
	unsigned long	pagesz,
	const char *	name
)
{
	struct region *rgn;
	struct region *cur;
	struct list_head *pos;
	unsigned long end = calc_end(start, extent);

	/* Region must have non-zero size */
	if (extent == 0) {
		printk(KERN_WARNING "Extent must be non-zero.\n");
		return -EINVAL;
	}

	/* Region must have a positive size */
	if (start >= end) {
		printk(KERN_WARNING
			"Invalid region size (start=0x%lx, extent=0x%lx).\n",
			start, extent);
		return -EINVAL;
	}

	/* Architecture must support the page size specified */
	if ((pagesz & supported_pagesz_mask) == 0) {
		printk(KERN_WARNING
			"Invalid page size specified (pagesz=0x%lx).\n",
			pagesz);
		return -EINVAL;
	}
	pagesz &= supported_pagesz_mask;

	/* Only one page size may be specified */
	if (!is_power_of_2(pagesz)) {
		printk(KERN_WARNING
			"More than one page size specified (pagesz=0x%lx).\n",
			pagesz);
		return -EINVAL;
	}

	/* Region must be aligned to at least the specified page size */
	if ((start & (pagesz-1)) || ((end!=ULONG_MAX) && (end & (pagesz-1)))) {
		printk(KERN_WARNING
			"Region is misaligned (start=0x%lx, end=0x%lx).\n",
			start, end);
		return -EINVAL;
	}

	/* Region must not overlap with any existing regions */
	list_for_each_entry(cur, &aspace->region_list, link) {
		if ((start < cur->end) && (end >= cur->start)) {
			printk(KERN_WARNING
				"Region overlaps with existing region.\n");
			return -EINVAL;
		}
	}

	/* Allocate and initialize a new region object */
	if ((rgn = kmem_alloc(sizeof(struct region))) == NULL)
		return -ENOMEM;

	rgn->aspace = aspace;
	rgn->start  = start;
	rgn->end    = end;
	rgn->flags  = flags;
	rgn->pagesz = pagesz;

	if (name) {
		memcpy(rgn->name, name, sizeof(rgn->name));
		rgn->name[sizeof(rgn->name)-1] = '\0';
	}

	/* Insert region into address space's sorted region list */
	list_for_each(pos, &aspace->region_list) {
		cur = list_entry(pos, struct region, link);
		if (cur->start > rgn->start)
			break;
	}
	list_add_tail(&rgn->link, pos);

	return 0;
}


/**
 * Deletes a region from an address space.
 *
 * Arguments:
 *       [IN] aspace: Address space to remove region from.
 *       [IN] start:  Starting address of the region being removed.
 *       [IN] extent: Size of the region being removed, in bytes.
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, the region was not removed.
 */
int
aspace_del_region(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	extent
)
{
	struct region *rgn;
	unsigned long end = calc_end(start, extent);
	int status;

	/* Locate the region to delete */
	rgn = find_region(aspace, start);
	if (!rgn || (rgn->start != start) || (rgn->end != end))
		return -EINVAL;

	/* Unmap all of the memory that was mapped to the region */
	status = aspace_unmap_memory(aspace, start, extent);
	if (status)
		return status;

	/* Remove the region from the address space */
	list_del(&rgn->link);
	kmem_free(rgn);
	return 0;
}


/**
 * Maps memory into an address space. The memory must be mapped to valid
 * regions of the address space (i.e., regions added with aspace_add_region()).
 *
 * Arguments:
 *       [IN] aspace: Address space to map memory into.
 *       [IN] start:  Starting address in the aspace to start mapping from.
 *       [IN] paddr:  Starting physical address of memory to map.
 *       [IN] extent: Number of bytes to map.
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, some of the memory may have been mapped, YMMV.
 */
int
aspace_map_memory(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	paddr,
	unsigned long	extent
)
{
	struct region *rgn;
	int status;

	while (extent) {
		/* Find region covering the address */
		rgn = find_region(aspace, start);
		if (!rgn) {
			printk(KERN_WARNING
				"Failed to find region covering addr=0x%lx.\n",
				start);
			return -EINVAL;
		}

		/* addresses must be aligned to region's page size */
		if ((start & (rgn->pagesz-1)) || (paddr & (rgn->pagesz-1))) {
			printk(KERN_WARNING
				"Misalignment "
				"(start=0x%lx, paddr=0x%lx, pagesz=0x%lx).\n",
				start, paddr, rgn->pagesz);
			return -EINVAL;
		}

		/* Map until full extent mapped or end of region is reached */
		while (extent && (start < rgn->end)) {

			status = 
			arch_aspace_map_page(
				aspace,
				start,
				paddr,
				rgn->flags,
				rgn->pagesz
			);
			if (status)
				return status;

			extent -= rgn->pagesz;
			start  += rgn->pagesz;
			paddr  += rgn->pagesz;
		}
	}

	return 0;
}


/**
 * Unmaps memory from an address space.
 *
 * Arguments:
 *       [IN] aspace: Address space to unmap memory from.
 *       [IN] start:  Starting address in the aspace to begin unmapping at.
 *       [IN] extent: Number of bytes to unmap.
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, some of the memory may have been unmapped, YMMV.
 */
int
aspace_unmap_memory(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	extent
)
{
	struct region *rgn;

	while (extent) {
		/* Find region covering the address */
		rgn = find_region(aspace, start);
		if (!rgn) {
			printk(KERN_WARNING
				"Failed to find region covering addr=0x%lx.\n",
				start);
			return -EINVAL;
		}

		/* address must be aligned to region's page size */
		if (start & (rgn->pagesz-1)) {
			printk(KERN_WARNING
				"Misalignment (start=0x%lx, pagesz=0x%lx).\n",
				start, rgn->pagesz);
			return -EINVAL;
		}

		/* Unmap until full extent unmapped or end of region is reached */
		while (extent && (start < rgn->end)) {

			arch_aspace_unmap_page(
				aspace,
				start,
				rgn->pagesz
			);

			extent -= rgn->pagesz;
			start  += rgn->pagesz;
		}
	}

	return 0;
}


/**
 * Convenience function that adds the specified region to an address space,
 * allocates memory for it using kmem_alloc(), and maps the memory into the
 * address space.
 *
 * Arguments:
 *       [IN] aspace: Address space to add the region to.
 *       [IN] start:  Starting address of region, must be pagesz aligned.
 *       [IN] extent: Size of the region in bytes, must be multiple of pagesz.
 *       [IN] flags:  Permissions, memory type, etc. (e.g., VM_WRITE | VM_READ).
 *       [IN] pagesz: Page size that should be used to map the region.
 *       [IN] name:   Human readable name of the region.
 *
 * Returns:
 *       Success: 0
 *       Failure: Error Code, address space may be partially modified, YMMV.
 */
int
aspace_kmem_alloc_region(
	struct aspace *	aspace,
	unsigned long	start,
	unsigned long	extent,
	unsigned long	flags,
	unsigned long	pagesz,
	const char *	name
)
{
	int status;
	void *mem;

	/* Add the region to the address space */
	status = aspace_add_region(
			aspace,
			start,
			extent,
			flags,
			pagesz,
			name
	);
	if (status)
		return status;

	/* Allocate memory for the region */
	mem = kmem_get_pages(ilog2(roundup_pow_of_two(extent/pagesz)));
	if (mem == NULL)
		return -ENOMEM;

	/* Map the memory to the region */
	status = aspace_map_memory(
			aspace,
			start,
			__pa(mem),
			extent
	);
	if (status)
		return status;

	return 0;
}


/**
 * Dumps the state of an address space object to the console.
 */
void
aspace_dump(struct aspace *aspace)
{
	struct region *rgn;

	printk(KERN_DEBUG "ADDRESS SPACE DUMP:\n");
	printk(KERN_DEBUG "  refcnt:  %d\n", aspace->refcnt);
	printk(KERN_DEBUG "  regions:\n");
	list_for_each_entry(rgn, &aspace->region_list, link) {
		printk(KERN_DEBUG
			"    [0x%016lx, 0x%016lx%c %s\n",
			rgn->start,
			rgn->end,
			(rgn->end == ULONG_MAX) ? ']' : ')',
			rgn->name
		);
	}
}


