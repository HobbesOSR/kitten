/* Copyright (c) 2007,2008 Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/spinlock.h>
#include <lwk/string.h>
#include <lwk/aspace.h>
#include <lwk/htable.h>
#include <lwk/log2.h>
#include <lwk/cpuinfo.h>
#include <lwk/pmem.h>
#include <lwk/tlbflush.h>
#include <lwk/waitq.h>
#include <lwk/sched.h>

/**
 * Hash table used to lookup address space structures by ID.
 */
static struct htable *htable;
static DEFINE_SPINLOCK(htable_lock);

/**
 * Where to start looking for ANY_ID create address space requests.
 */
static id_t aspace_next_id = UASPACE_MIN_ID;

/**
 * Memory region structure. A memory region represents a contiguous region 
 * [start, end) of valid memory addresses in an address space.
 *
 * \note This structure is opaque to users of the aspace API and is
 * used for internal bookkeeping.
 */
struct region
{
	struct aspace *  aspace;   /**< Address space this region belongs to */
	struct list_head link;     /**< Linkage in the aspace->region_list */

	vaddr_t          start;    /**< Starting address of the region */
	vaddr_t          end;      /**< 1st byte after end of the region */
	vmflags_t        flags;    /**< Permissions, caching, etc. */
	vmpagesize_t     pagesz;   /**< Allowed page sizes... 2^bit */
	id_t             smartmap; /**< If (flags & VM_SMARTMAP), ID of the
	                              aspace this region is mapped to */
	char             name[16]; /**< Human-readable name of the region */
};


/**
 * This calculates a region's end address. Normally end is the address of the
 * first byte after the region. However if the region extends to the end of
 * memory, that is not possible so set end to the last valid address,
 * ULONG_MAX.
 */
static vaddr_t
calc_end(vaddr_t start, size_t extent)
{
	vaddr_t end = start + extent;
	if (end == 0)
		end = ULONG_MAX;
	return end;
}

/**
 * Locates the region covering the specified address.
 */
static struct region *
find_region(struct aspace *aspace, vaddr_t addr)
{
	struct region *rgn;
	
	list_for_each_entry(rgn, &aspace->region_list, link) {
		if ((rgn->start <= addr) && (rgn->end > addr))
			return rgn;
	}
	return NULL;
}

/**
 * Finds a region that overlaps the specified interval.
 */
static struct region *
find_overlapping_region(struct aspace *aspace, vaddr_t start, vaddr_t end)
{
	struct region *rgn;

	list_for_each_entry(rgn, &aspace->region_list, link) {
		if ((start < rgn->end) && (end > rgn->start))
			return rgn;
	}
	return NULL;
}

/**
 * Locates the region that is SMARTMAP'ed to the specified aspace ID.
 */
static struct region *
find_smartmap_region(struct aspace *aspace, id_t src_aspace)
{
	struct region *rgn;
	
	list_for_each_entry(rgn, &aspace->region_list, link) {
		if ((rgn->flags & VM_SMARTMAP) && (rgn->smartmap == src_aspace))
			return rgn;
	}
	return NULL;
}

/**
 * Looks up an aspace object by ID and returns it with its spinlock locked.
 */
static struct aspace *
lookup_and_lock(id_t id)
{
	struct aspace *aspace;

	/* Lock the hash table, lookup aspace object by ID */
	spin_lock(&htable_lock);
	if ((aspace = htable_lookup(htable, &id)) == NULL) {
		spin_unlock(&htable_lock);
		return NULL;
	}

	/* Lock the identified aspace */
	spin_lock(&aspace->lock);

	/* Unlock the hash table, others may now use it */
	spin_unlock(&htable_lock);

	return aspace;
}

/**
 * Like lookup_and_lock(), but looks up two address spaces instead of one.
 */
static int
lookup_and_lock_two(id_t a, id_t b,
                    struct aspace **aspace_a, struct aspace **aspace_b)
{
	/* As a convenience, handle case where a and b are the same */
	if (a == b) {
		if ((*aspace_a = lookup_and_lock(a)) == NULL)
			return -ENOENT;
		*aspace_b = *aspace_a;
		return 0;
	}

	/* Lock the hash table, lookup aspace objects by ID */
	spin_lock(&htable_lock);
	if ((*aspace_a = htable_lookup(htable, &a)) == NULL) {
		spin_unlock(&htable_lock);
		return -ENOENT;
	}

	if ((*aspace_b = htable_lookup(htable, &b)) == NULL) {
		spin_unlock(&htable_lock);
		return -ENOENT;
	}

	/* Lock the identified aspaces */
	spin_lock(&(*aspace_a)->lock);
	spin_lock(&(*aspace_b)->lock);

	/* Unlock the hash table, others may now use it */
	spin_unlock(&htable_lock);

	return 0;
}

/**
 * Initialize the address space subsystem.
 *
 * Allocate the hash tables for address space lookups, create
 * the kernel thread address spaces and make them active.
 */
int __init
aspace_subsys_init(void)
{
	int status;

	/* Create a hash table that will be used for quick ID->aspace lookups */
	htable = htable_create(
			7,  /* 2^7 bins in the hash table */
			offsetof(struct aspace, id),
			offsetof(struct aspace, ht_link),
			htable_id_hash,
			htable_id_key_compare
	);
	if (!htable)
		panic("Failed to create aspace hash table.");

	/* Create an aspace for use by kernel threads */
	if ((status = aspace_create(KERNEL_ASPACE_ID, "kernel", NULL)))
		panic("Failed to create kernel aspace (status=%d).", status);

	/* Switch to the newly created kernel address space */
	if ((current->aspace = aspace_acquire(KERNEL_ASPACE_ID)) == NULL)
		panic("Failed to acquire kernel aspace.");
	arch_aspace_activate(current->aspace);

	return 0;
}

int
aspace_get_myid(id_t *id)
{
	*id = current->aspace->id;
	return 0;
}

static id_t
id_inc(id_t id)
{
	if (++id > UASPACE_MAX_ID)
		id = UASPACE_MIN_ID;
	return id;
}

static id_t
id_alloc_any(void)
{
	id_t id;
	id_t stop_id = aspace_next_id;

	while (htable_lookup(htable, &aspace_next_id) != NULL) {
		aspace_next_id = id_inc(aspace_next_id);
		if (aspace_next_id == stop_id)
			return ERROR_ID;
	}

	id = aspace_next_id;
	aspace_next_id = id_inc(aspace_next_id);

	return id;
}

static id_t
id_alloc_specific(id_t id)
{
	if (htable_lookup(htable, &id) != NULL)
		return ERROR_ID;

	return id;
}

int
aspace_create(id_t id_request, const char *name, id_t *id)
{
	int status, i;
	id_t new_id;
	struct aspace *aspace;
	unsigned long irqstate;

	spin_lock_irqsave(&htable_lock, irqstate);

	if (id_request == KERNEL_ASPACE_ID) {
		new_id = KERNEL_ASPACE_ID;
	} else {
		new_id = (id_request == ANY_ID)
				? id_alloc_any()
				: id_alloc_specific(id_request);
		if (new_id == ERROR_ID) {
			status = -EEXIST;
			goto fail_id_alloc;
		}
	}

	if ((aspace = kmem_alloc(sizeof(*aspace))) == NULL) {
		status = -ENOMEM;
		goto fail_aspace_alloc;
	}

	/*
	 * Initialize the address space. kmem_alloc() allocates zeroed memory
	 * so fields with an initial state of zero do not need to be explicitly
	 * initialized.
	 */
	aspace->id = new_id;
	spin_lock_init(&aspace->lock);
	list_head_init(&aspace->region_list);
	hlist_node_init(&aspace->ht_link);
	sema_init(&aspace->mmap_sem, 1);
	if (name)
		strlcpy(aspace->name, name, sizeof(aspace->name));

	list_head_init(&aspace->task_list);
	aspace->next_task_id = aspace->id;

	aspace->cpu_mask = cpu_present_map;
	aspace->next_cpu_id = first_cpu(aspace->cpu_mask);

	list_head_init(&aspace->sigpending.list);

	aspace->parent = current->aspace;
	list_head_init(&aspace->child_list);
	waitq_init(&aspace->child_exit_waitq);

	spin_lock(&current->aspace->lock);
	list_add_tail(&aspace->child_link, &current->aspace->child_list);
	spin_unlock(&current->aspace->lock);

	/* Create a region for the kernel portion of the address space */
	status =
	__aspace_add_region(
		aspace,
		PAGE_OFFSET,
		ULONG_MAX-PAGE_OFFSET+1, /* # bytes to end of memory */
		VM_KERNEL,
		PAGE_SIZE,
		"kernel"
	);
	if (status)
		goto fail_add_region;

	/* Initialize futex queues, used to hold addr space private futexes */
	for (i = 0; i < ARRAY_SIZE(aspace->futex_queues); i++)
		futex_queue_init(&aspace->futex_queues[i]);

	/* Do architecture-specific initialization */
	if ((status = arch_aspace_create(aspace)) != 0)
		goto fail_arch;

	/* Add new address space to a hash table, for quick lookups by ID */
	htable_add(htable, aspace);

	if (id)
		*id = aspace->id;

	spin_unlock_irqrestore(&htable_lock, irqstate);
	return 0;

fail_arch:
fail_add_region:
	kmem_free(aspace);
fail_aspace_alloc:
fail_id_alloc:
	spin_unlock_irqrestore(&htable_lock, irqstate);
	return status;
}

int
aspace_destroy(id_t id)
{
	struct aspace *aspace;
	struct list_head *pos, *tmp;
	struct region *rgn;
	unsigned long irqstate;

	/* Lock the hash table, lookup aspace object by ID */
	spin_lock_irqsave(&htable_lock, irqstate);
	if ((aspace = htable_lookup(htable, &id)) == NULL) {
		spin_unlock_irqrestore(&htable_lock, irqstate);
		return -EINVAL;
	}

	/* Lock the identified aspace */
	spin_lock(&aspace->lock);

	if (aspace->refcnt || !list_empty(&aspace->child_list)) {
		spin_unlock(&aspace->lock);
		spin_unlock_irqrestore(&htable_lock, irqstate);
		return -EBUSY;
	}

	/* Remove aspace from hash table, preventing others from finding it */
	htable_del(htable, aspace);

	/* Unlock the destroyed aspace, we are the only users of it now */
	spin_unlock(&aspace->lock);

	/* Remove the destroyed aspace from its parent's child_list */
	spin_lock(&aspace->parent->lock);
	list_del(&aspace->child_link);
	spin_unlock(&aspace->parent->lock);

	/* Unlock the hash table, others may now use it */
	spin_unlock_irqrestore(&htable_lock, irqstate);
 
	/* Finish up destroying the aspace, we have the only reference */
	list_for_each_safe(pos, tmp, &aspace->region_list) {
		rgn = list_entry(pos, struct region, link);
		/* Must drop our reference on all SMARTMAP'ed aspaces */
		if (rgn->flags & VM_SMARTMAP) {
			struct aspace *src;
			spin_lock_irqsave(&htable_lock, irqstate);
			src = htable_lookup(htable, &rgn->smartmap);
			BUG_ON(src == NULL);
			spin_lock(&src->lock);
			--src->refcnt;
			spin_unlock(&src->lock);
			spin_unlock_irqrestore(&htable_lock, irqstate);
		}
		list_del(&rgn->link);
		kmem_free(rgn);
	}
	arch_aspace_destroy(aspace);
	kmem_free(aspace);
	return 0;
}

/**
 * Acquires an address space object. The object is guaranteed not to be
 * deleted until it is released via aspace_release().
 */
struct aspace *
aspace_acquire(id_t id)
{
	struct aspace *aspace;
	unsigned long irqstate;

	local_irq_save(irqstate);
	if ((aspace = lookup_and_lock(id)) != NULL) {
		++aspace->refcnt;
		spin_unlock(&aspace->lock);
	}
	local_irq_restore(irqstate);
	return aspace;
}


int 
aspace_update_cpumask(id_t id, cpumask_t * cpu_mask) 
{
	struct aspace *aspace;
	unsigned long irqstate;
	int ret = 0;

	local_irq_save(irqstate);
	if ((aspace = lookup_and_lock(id)) != NULL) {
		aspace->cpu_mask = *cpu_mask;
		aspace->next_cpu_id = first_cpu(aspace->cpu_mask);
		spin_unlock(&aspace->lock);
	} else {
	    ret = -1;
	}
	local_irq_restore(irqstate);

	return ret;
}


/**
 * Releases an aspace object that was previously acquired via aspace_acquire().
 * The aspace object passed in must be unlocked.
 */
void
aspace_release(struct aspace *aspace)
{
	unsigned long irqstate;
	spin_lock_irqsave(&aspace->lock, irqstate);
	--aspace->refcnt;
	spin_unlock_irqrestore(&aspace->lock, irqstate);
}

int
__aspace_find_hole(struct aspace *aspace,
                   vaddr_t start_hint, size_t extent, size_t alignment,
                   vaddr_t *start)
{
	struct region *rgn;
	vaddr_t hole;

	if (!aspace || !extent || !is_power_of_2(alignment))
		return -EINVAL;

	if (start_hint == 0)
		start_hint = 1;

	hole = round_up(start_hint, alignment);
	while ((rgn = find_overlapping_region(aspace, hole, hole + extent))) {
		if (rgn->end == ULONG_MAX)
			return -ENOENT;
		hole = round_up(rgn->end, alignment);
	}
	
	if (start)
		*start = hole;
	return 0;
}

int
aspace_find_hole(id_t id,
                 vaddr_t start_hint, size_t extent, size_t alignment,
                 vaddr_t *start)
{
	int status;
	struct aspace *aspace;
	unsigned long irqstate;

	local_irq_save(irqstate);
	aspace = lookup_and_lock(id);
	status = __aspace_find_hole(aspace, start_hint, extent, alignment,
	                            start);
	if (aspace) spin_unlock(&aspace->lock);
	local_irq_restore(irqstate);
	return status;
}


int
__aspace_add_region(struct aspace *aspace,
                    vaddr_t start, size_t extent,
                    vmflags_t flags, vmpagesize_t pagesz,
                    const char *name)
{
	struct region *rgn;
	struct region *cur;
	struct list_head *pos;
	vaddr_t end = calc_end(start, extent);

	if (!aspace || !start)
		return -EINVAL;

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
	if ((pagesz & cpu_info[0].pagesz_mask) == 0) {
		printk(KERN_WARNING
			"Invalid page size specified (pagesz=0x%lx).\n",
			pagesz);
		return -EINVAL;
	}
	pagesz &= cpu_info[0].pagesz_mask;

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
		if ((start < cur->end) && (end > cur->start)) {
			printk(KERN_WARNING
			       "Region overlaps with existing region.\n");
			return -ENOTUNIQ;
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
	if (name)
		strlcpy(rgn->name, name, sizeof(rgn->name));

	/* The heap region is special, remember its bounds */
	if (flags & VM_HEAP) {
		aspace->heap_start = start;
		aspace->heap_end   = end;
		aspace->brk        = aspace->heap_start;
		aspace->mmap_brk   = aspace->heap_end;
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

int
aspace_add_region(id_t id,
                  vaddr_t start, size_t extent,
                  vmflags_t flags, vmpagesize_t pagesz,
                  const char *name)
{
	int status;
	struct aspace *aspace;
	unsigned long irqstate;

	local_irq_save(irqstate);
	aspace = lookup_and_lock(id);
	status = __aspace_add_region(aspace, start, extent, flags, pagesz, name);
	if (aspace) spin_unlock(&aspace->lock);
	local_irq_restore(irqstate);
	return status;
}

int
__aspace_del_region(struct aspace *aspace, vaddr_t start, size_t extent)
{
	int status;
	struct region *rgn;
	vaddr_t end = calc_end(start, extent);

	if (!aspace)
		return -EINVAL;

	/* Locate the region to delete */
	rgn = find_region(aspace, start);
	if (!rgn || (rgn->start != start) || (rgn->end != end)
	     || (rgn->flags & VM_KERNEL))
		return -EINVAL;

	if (!(rgn->flags & VM_SMARTMAP)) {
		/* Unmap all of the memory that was mapped to the region */
		status = __aspace_unmap_pmem(aspace, start, extent);
		if (status)
			return status;
	}

	/* Remove the region from the address space */
	list_del(&rgn->link);
	kmem_free(rgn);
	return 0;
}

int
aspace_del_region(id_t id, vaddr_t start, size_t extent)
{
	int status;
	struct aspace *aspace;
	unsigned long irqstate;

	local_irq_save(irqstate);
	aspace = lookup_and_lock(id);
	status = __aspace_del_region(aspace, start, extent);
	if (aspace) spin_unlock(&aspace->lock);
	local_irq_restore(irqstate);
	flush_tlb();
	return status;
}

int
__aspace_map_pmem(struct aspace *aspace,
                  paddr_t pmem, vaddr_t start, size_t extent)
{
	int status;
	struct region *rgn;

	if (!aspace)
		return -EINVAL;

	if ((get_cpu_var(umem_only) == true) &&
	    (pmem_is_type(PMEM_TYPE_UMEM, pmem, extent) == false) &&
	    (pmem_is_type(PMEM_TYPE_INIT_TASK, pmem, extent) == false))
		return -EPERM;

	while (extent) {
		/* Find region covering the address */
		rgn = find_region(aspace, start);
		if (!rgn) {
			printk(KERN_WARNING
				"Failed to find region covering addr=0x%lx.\n",
				start);
			return -EINVAL;
		}

		/* Can't map anything to kernel or SMARTMAP regions */
		if ((rgn->flags & VM_KERNEL) || (rgn->flags & VM_SMARTMAP)) {
			printk(KERN_WARNING
				"Trying to map memory to protected region.\n");
			return -EINVAL;
		}

		/* addresses must be aligned to region's page size */
		if ((start & (rgn->pagesz-1)) || (pmem & (rgn->pagesz-1))) {
			printk(KERN_WARNING
				"Misalignment "
				"(start=0x%lx, pmem=0x%lx, pagesz=0x%lx).\n",
				start, pmem, rgn->pagesz);
			return -EINVAL;
		}

		/* Map until full extent mapped or end of region is reached */
		while (extent && (start < rgn->end)) {

			status = 
			arch_aspace_map_page(
				aspace,
				start,
				pmem,
				rgn->flags,
				rgn->pagesz
			);
			if (status)
				return status;

			extent -= rgn->pagesz;
			start  += rgn->pagesz;
			pmem   += rgn->pagesz;
		}
	}

	return 0;
}

int
aspace_map_pmem(id_t id, paddr_t pmem, vaddr_t start, size_t extent)
{
	int status;
	struct aspace *aspace;
	unsigned long irqstate;

	local_irq_save(irqstate);
	aspace = lookup_and_lock(id);
	status = __aspace_map_pmem(aspace, pmem, start, extent);
	if (aspace) spin_unlock(&aspace->lock);
	local_irq_restore(irqstate);
	return status;
}

int
__aspace_unmap_pmem(struct aspace *aspace, vaddr_t start, size_t extent)
{
	struct region *rgn;

	if (!aspace)
		return -EINVAL;

	while (extent) {
		/* Find region covering the address */
		rgn = find_region(aspace, start);
		if (!rgn) {
			printk(KERN_WARNING
				"Failed to find region covering addr=0x%lx.\n",
				start);
			return -EINVAL;
		}

		/* Can't unmap anything from kernel or SMARTMAP regions */
		if ((rgn->flags & VM_KERNEL) || (rgn->flags & VM_SMARTMAP)) {
			printk(KERN_WARNING
				"Trying to map memory to protected region.\n");
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

int
aspace_unmap_pmem(id_t id, vaddr_t start, size_t extent)
{
	int status;
	struct aspace *aspace;
	unsigned long irqstate;

	local_irq_save(irqstate);
	aspace = lookup_and_lock(id);
	status = __aspace_unmap_pmem(aspace, start, extent);
	if (aspace) spin_unlock(&aspace->lock);
	local_irq_restore(irqstate);
	flush_tlb();
	return status;
}

int
__aspace_smartmap(struct aspace *src, struct aspace *dst,
                  vaddr_t start, size_t extent)
{
	int status;
	vaddr_t end = start + extent;
	char name[16];
	struct region *rgn;

	/* Can only SMARTMAP a given aspace in once */
	if (find_smartmap_region(dst, src->id))
		return -EINVAL;

	if (start >= end)
		return -EINVAL;

	if ((start & (SMARTMAP_ALIGN-1)) || (end & (SMARTMAP_ALIGN-1)))
		return -EINVAL;

	snprintf(name, sizeof(name), "SMARTMAP-%u", (unsigned int)src->id);
	if ((status = __aspace_add_region(dst, start, extent,
	                                  VM_SMARTMAP, PAGE_SIZE, name)))
		return status;

	/* Do architecture-specific SMARTMAP initialization */
	if ((status = arch_aspace_smartmap(src, dst, start, extent))) {
		BUG_ON(__aspace_del_region(dst, start, extent));
		return status;
	}

	/* Remember the source aspace that the SMARTMAP region is mapped to */
	rgn = find_region(dst, start);
	BUG_ON(!rgn);
	rgn->smartmap = src->id;

	/* Ensure source aspace doesn't go away while we have it SMARTMAP'ed */
	++src->refcnt;

	return 0;
}

int
aspace_smartmap(id_t src, id_t dst, vaddr_t start, size_t extent)
{
	int status;
	struct aspace *src_spc, *dst_spc;
	unsigned long irqstate;

	local_irq_save(irqstate);

	if ((status = lookup_and_lock_two(src, dst, &src_spc, &dst_spc))) {
		local_irq_restore(irqstate);
		return status;
	}

	status = __aspace_smartmap(src_spc, dst_spc, start, extent);

	spin_unlock(&src_spc->lock);
	if (src != dst)
		spin_unlock(&dst_spc->lock);

	local_irq_restore(irqstate);
	return status;
}

int
__aspace_unsmartmap(struct aspace *src, struct aspace *dst)
{
	struct region *rgn;
	size_t extent;

	if ((rgn = find_smartmap_region(dst, src->id)) == NULL)
		return -EINVAL;
	extent = rgn->end - rgn->start;

	/* Do architecture-specific SMARTMAP unmapping */
	BUG_ON(arch_aspace_unsmartmap(src, dst, rgn->start, extent));

	/* Delete the SMARTMAP region and release our reference on the source */
	BUG_ON(__aspace_del_region(dst, rgn->start, extent));
	--src->refcnt;

	return 0;
}

int
aspace_unsmartmap(id_t src, id_t dst)
{
	int status;
	struct aspace *src_spc, *dst_spc;
	unsigned long irqstate;

	local_irq_save(irqstate);

	if ((status = lookup_and_lock_two(src, dst, &src_spc, &dst_spc))) {
		local_irq_restore(irqstate);
		return status;
	}

	status = __aspace_unsmartmap(src_spc, dst_spc);

	spin_unlock(&src_spc->lock);
	if (src != dst)
		spin_unlock(&dst_spc->lock);

	local_irq_restore(irqstate);
	flush_tlb();
	return status;
}

int
__aspace_virt_to_phys(struct aspace *aspace, vaddr_t vaddr, paddr_t *paddr)
{
	if (!aspace)
		return -EINVAL;
	return arch_aspace_virt_to_phys(aspace, vaddr, paddr);
}

int
aspace_virt_to_phys(id_t id, vaddr_t vaddr, paddr_t *paddr)
{
	int status;
	struct aspace *aspace;
	unsigned long irqstate;

	if (id == MY_ID)
		id = current->aspace->id;

	local_irq_save(irqstate);
	aspace = lookup_and_lock(id);
	status = __aspace_virt_to_phys(aspace, vaddr, paddr);
	if (aspace) spin_unlock(&aspace->lock);
	local_irq_restore(irqstate);

	return status;
}


int 
__aspace_lookup_mapping(struct aspace *aspace, vaddr_t vaddr, aspace_mapping_t *mapping)
{
	struct region * rgn;

	if (!aspace) {
		return -EINVAL;
	}
	
	rgn = find_region(aspace, vaddr);
	if (!rgn) {
		printk(KERN_WARNING
		       "Failed to find region covering addr=0x%lx.\n",
		       vaddr);
		return -EINVAL;
	}
	

	mapping->start = rgn->start;
	mapping->end = rgn->end;
	mapping->flags = rgn->flags;

	return __aspace_virt_to_phys(aspace, mapping->start, &(mapping->paddr));
}

int 
aspace_lookup_mapping(id_t id, vaddr_t vaddr, aspace_mapping_t *mapping)
{
	int status;
	struct aspace * aspace;
	unsigned long irqstate;

	if (id == MY_ID) {
		id = current->aspace->id;
	}

	local_irq_save(irqstate);
	aspace = lookup_and_lock(id);
	status = __aspace_lookup_mapping(aspace, vaddr, mapping);
	if (aspace) spin_unlock(&aspace->lock);
	local_irq_restore(irqstate);

	return status;
}


int
aspace_wait4_child_exit(id_t child_id, bool block, id_t *exit_id, int *exit_status)
{
	DECLARE_WAITQ_ENTRY(wait, current);
	unsigned long irqstate;
	struct aspace *child;
	size_t unreaped_children;
	bool found_exited_child, found_child_id;
	int saved_id = 0, saved_exit_status = 0;
	int status;

	waitq_add_entry(&current->aspace->child_exit_waitq, &wait);
repeat:
	// Setup initial conditions
	found_exited_child = false;
	found_child_id     = false;
	unreaped_children  = 0;

	set_task_state(current, TASK_INTERRUPTIBLE);

	// Taking htable_lock prevents new aspaces from being created,
	// which guarantees that no children are added to current's child_list
	spin_lock_irqsave(&htable_lock, irqstate);

	list_for_each_entry(child, &current->aspace->child_list, child_link) {
		spin_lock(&child->lock);

		if (child->reaped == true) {
			spin_unlock(&child->lock);
			continue;
		}

		++unreaped_children;

		if (child_id != ANY_ID) {
			if (child->id != child_id) {
				spin_unlock(&child->lock);
				continue;
			} else {
				found_child_id = true;
			}
		}

		if (child->exiting && list_empty(&child->task_list)) {
			found_exited_child = true;
			saved_id           = child->id;
			saved_exit_status  = child->exit_status;
			child->reaped      = true;
		}

		spin_unlock(&child->lock);

		if (found_exited_child || found_child_id)
			break;
	}

	spin_unlock_irqrestore(&htable_lock, irqstate);

	// Have all of the information needed to know if we should sleep or return

	if (unreaped_children == 0) {
		status = -ECHILD;
		goto end;
	}

	if ((child_id != ANY_ID) && (found_child_id == false)) {
		status = -ECHILD;
		goto end;
	}

	if (found_exited_child == false) {
		if (block == false) {
			status = -EAGAIN;
			goto end;
		}

		schedule();

		if (signal_pending(current)) {
			status = -EINTR;
			goto end;
		}

		goto repeat;
	}

	// Success, found_exited_child == true

	if (exit_id)
		*exit_id = saved_id;

	if (exit_status)
		*exit_status = saved_exit_status;

	status = 0;

end:
	set_task_state(current, TASK_RUNNING);
	waitq_remove_entry(&current->aspace->child_exit_waitq, &wait);
	return status;
}

int
aspace_dump2console(id_t id)
{
	struct aspace *aspace;
	struct region *rgn;
	unsigned long irqstate;

	local_irq_save(irqstate);

	if ((aspace = lookup_and_lock(id)) == NULL) {
		local_irq_restore(irqstate);
		return -EINVAL;
	}

	printk(KERN_DEBUG "DUMP OF ADDRESS SPACE %u:\n", aspace->id);
	printk(KERN_DEBUG "  name:    %s\n", aspace->name);
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

	spin_unlock(&aspace->lock);
	local_irq_restore(irqstate);
	return 0;
}


int
aspace_set_rank(id_t id,
                id_t rank)
{
	struct aspace *aspace;
	unsigned long irqstate;

	local_irq_save(irqstate);

	if ((aspace = lookup_and_lock(id)) == NULL) {
		local_irq_restore(irqstate);
		return -EINVAL;
	}

	aspace->rank = rank;

	spin_unlock(&aspace->lock);
	local_irq_restore(irqstate);
	return 0;
}

int
aspace_get_rank(id_t   id,
		id_t * rank)
{
	struct aspace *aspace;
	unsigned long irqstate;

	local_irq_save(irqstate);

	if ((aspace = lookup_and_lock(id)) == NULL) {
		local_irq_restore(irqstate);
		return -EINVAL;
	}
	
	*rank = aspace->rank;

	spin_unlock(&aspace->lock);
	local_irq_restore(irqstate);
	return 0;
}
