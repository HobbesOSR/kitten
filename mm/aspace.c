/* Copyright (c) 2007,2008 Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/spinlock.h>
#include <lwk/string.h>
#include <lwk/aspace.h>
#include <lwk/idspace.h>
#include <lwk/htable.h>
#include <lwk/log2.h>
#include <lwk/cpuinfo.h>
#include <arch/uaccess.h>

/**
 * ID space used to allocate address space IDs.
 */
static idspace_t idspace;

/**
 * Hash table used to lookup address space structures by ID.
 */
static htable_t htable;

/**
 * Lock for serializing access to the htable.
 */
static DEFINE_SPINLOCK(htable_lock);

/**
 * Memory region structure. A memory region represents a contiguous region 
 * [start, end) of valid memory addresses in an address space.
 */
struct region {
	struct aspace *  aspace;   /* Address space this region belongs to */
	struct list_head link;     /* Linkage in the aspace->region_list */

	vaddr_t          start;    /* Starting address of the region */
	vaddr_t          end;      /* 1st byte after end of the region */
	vmflags_t        flags;    /* Permissions, caching, etc. */
	vmpagesize_t     pagesz;   /* Allowed page sizes... 2^bit */
	id_t             smartmap; /* If (flags & VM_SMARTMAP), ID of the
	                              aspace this region is mapped to */
	char             name[16]; /* Human-readable name of the region */
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
	if ((aspace = htable_lookup(htable, id)) == NULL) {
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
	/* Lock the hash table, lookup aspace objects by ID */
	spin_lock(&htable_lock);
	if ((*aspace_a = htable_lookup(htable, a)) == NULL) {
		spin_unlock(&htable_lock);
		return -ENOENT;
	}

	if ((*aspace_b = htable_lookup(htable, b)) == NULL) {
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

static bool
id_ok(id_t id)
{
	return ((id >= ASPACE_MIN_ID) && (id <= ASPACE_MAX_ID));
}

int __init
aspace_subsys_init(void)
{
	int status;

	/* Create an ID space for allocating address space IDs */
	if ((status = idspace_create(__ASPACE_MIN_ID, __ASPACE_MAX_ID, &idspace)))
		panic("Failed to create aspace ID space (status=%d).", status);

	/* Create a hash table that will be used for quick ID->aspace lookups */
	if ((status = htable_create(7 /* 2^7 bins */,
	                            offsetof(struct aspace, id),
	                            offsetof(struct aspace, ht_link),
	                            &htable)))
		panic("Failed to create aspace hash table (status=%d).", status);

	/* Create an aspace for use by kernel threads */
	if ((status = aspace_create(KERNEL_ASPACE_ID, "kernel", NULL)))
		panic("Failed to create kernel aspace (status=%d).", status);

	return 0;
}

int
aspace_get_myid(id_t *id)
{
	*id = current->aspace->id;
	return 0;
}

int
sys_aspace_get_myid(id_t __user *id)
{
	int status;
	id_t _id;

	if ((status = aspace_get_myid(&_id)) != 0)
		return status;

	if (id && copy_to_user(id, &_id, sizeof(*id)))
		return -EINVAL;

	return 0;
}

int
aspace_create(id_t id_request, const char *name, id_t *id)
{
	int status;
	id_t new_id;
	struct aspace *aspace;
	unsigned long flags;

	if ((status = idspace_alloc_id(idspace, id_request, &new_id)) != 0)
		return status;

	if ((aspace = kmem_alloc(sizeof(*aspace))) == NULL) {
		idspace_free_id(idspace, new_id);
		return -ENOMEM;
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
	if (name)
		strlcpy(aspace->name, name, sizeof(aspace->name));

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
		goto error1;

	/* Do architecture-specific initialization */
	if ((status = arch_aspace_create(aspace)) != 0)
		goto error2;

	/* Add new address space to a hash table, for quick lookups by ID */
	spin_lock_irqsave(&htable_lock, flags);
	BUG_ON(htable_add(htable, aspace));
	spin_unlock_irqrestore(&htable_lock, flags);

	if (id)
		*id = new_id;
	return 0;

error2:
	BUG_ON(__aspace_del_region(aspace,PAGE_OFFSET,ULONG_MAX-PAGE_OFFSET+1));
error1:
	idspace_free_id(idspace, aspace->id);
	kmem_free(aspace);
	return status;
}

int
sys_aspace_create(id_t id_request, const char __user *name, id_t __user *id)
{
	int status;
	char _name[16];
	id_t _id;

	if (current->uid != 0)
		return -EPERM;

	if ((id_request != ANY_ID) && !id_ok(id_request))
		return -EINVAL;

	if (strncpy_from_user(_name, name, sizeof(_name)) < 0)
		return -EFAULT;
	_name[sizeof(_name) - 1] = '\0';

	if ((status = aspace_create(id_request, _name, &_id)) != 0)
		return status;

	BUG_ON(!id_ok(_id));

	if (id && copy_to_user(id, &_id, sizeof(*id)))
		return -EFAULT;

	return 0;
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
	if ((aspace = htable_lookup(htable, id)) == NULL) {
		spin_unlock_irqrestore(&htable_lock, irqstate);
		return -EINVAL;
	}

	/* Lock the identified aspace */
	spin_lock(&aspace->lock);

	if (aspace->refcnt) {
		spin_unlock(&aspace->lock);
		spin_unlock_irqrestore(&htable_lock, irqstate);
		return -EBUSY;
	}

	/* Remove aspace from hash table, preventing others from finding it */
	BUG_ON(htable_del(htable, aspace));

	/* Unlock the hash table, others may now use it */
	spin_unlock_irqrestore(&htable_lock, irqstate);
	spin_unlock(&aspace->lock);
 
	/* Finish up destroying the aspace, we have the only reference */
	list_for_each_safe(pos, tmp, &aspace->region_list) {
		rgn = list_entry(pos, struct region, link);
		/* Must drop our reference on all SMARTMAP'ed aspaces */
		if (rgn->flags & VM_SMARTMAP) {
			struct aspace *src;
			spin_lock_irqsave(&htable_lock, irqstate);
			src = htable_lookup(htable, rgn->smartmap);
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
	BUG_ON(idspace_free_id(idspace, aspace->id));
	kmem_free(aspace);
	return 0;
}

int
sys_aspace_destroy(id_t id)
{
	if (current->uid != 0)
		return -EPERM;
	if (!id_ok(id))
		return -EINVAL;
	return aspace_destroy(id);
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
__aspace_add_region(struct aspace *aspace,
                    vaddr_t start, size_t extent,
                    vmflags_t flags, vmpagesize_t pagesz,
                    const char *name)
{
	struct region *rgn;
	struct region *cur;
	struct list_head *pos;
	vaddr_t end = calc_end(start, extent);

	if (!aspace)
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
sys_aspace_add_region(id_t id,
                      vaddr_t start, size_t extent,
                      vmflags_t flags, vmpagesize_t pagesz,
                      const char __user *name)
{
	char _name[16];

	if (current->uid != 0)
		return -EPERM;

	if (!id_ok(id))
		return -EINVAL;

	if (strncpy_from_user(_name, name, sizeof(_name)) < 0)
		return -EFAULT;
	_name[sizeof(_name) - 1] = '\0';

	return aspace_add_region(id, start, extent, flags, pagesz, _name);
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
	return status;
}

int
sys_aspace_del_region(id_t id, vaddr_t start, size_t extent)
{
	if (current->uid != 0)
		return -EPERM;
	if (!id_ok(id))
		return -EINVAL;
	return aspace_del_region(id, start, extent);
}

int
__aspace_map_pmem(struct aspace *aspace,
                  paddr_t pmem, vaddr_t start, size_t extent)
{
	int status;
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
sys_aspace_map_pmem(id_t id, paddr_t pmem, vaddr_t start, size_t extent)
{
	if (current->uid != 0)
		return -EPERM;
	if (!id_ok(id))
		return -EINVAL;
	return aspace_map_pmem(id, pmem, start, extent);
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
	return status;
}

int
sys_aspace_unmap_pmem(id_t id, vaddr_t start, size_t extent)
{
	if (current->uid != 0)
		return -EPERM;
	if (!id_ok(id))
		return -EINVAL;
	return aspace_unmap_pmem(id, start, extent);
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

	/* Don't allow self SMARTMAP'ing */
	if (src == dst)
		return -EINVAL;

	local_irq_save(irqstate);
	if ((status = lookup_and_lock_two(src, dst, &src_spc, &dst_spc))) {
		local_irq_restore(irqstate);
		return status;
	}
	status = __aspace_smartmap(src_spc, dst_spc, start, extent);
	spin_unlock(&src_spc->lock);
	spin_unlock(&dst_spc->lock);
	local_irq_restore(irqstate);
	return status;
}

int
sys_aspace_smartmap(id_t src, id_t dst, vaddr_t start, size_t extent)
{
	if (current->uid != 0)
		return -EPERM;
	if (!id_ok(src) || !id_ok(dst))
		return -EINVAL;
	return aspace_smartmap(src, dst, start, extent);
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

	/* Don't allow self SMARTMAP'ing */
	if (src == dst)
		return -EINVAL;

	local_irq_save(irqstate);
	if ((status = lookup_and_lock_two(src, dst, &src_spc, &dst_spc))) {
		local_irq_restore(irqstate);
		return status;
	}
	status = __aspace_unsmartmap(src_spc, dst_spc);
	spin_unlock(&src_spc->lock);
	spin_unlock(&dst_spc->lock);
	local_irq_restore(irqstate);
	return status;
}

int
sys_aspace_unsmartmap(id_t src, id_t dst)
{
	if (current->uid != 0)
		return -EPERM;
	if (!id_ok(src) || !id_ok(dst))
		return -EINVAL;
	return aspace_unsmartmap(src, dst);
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
sys_aspace_dump2console(id_t id)
{
	return aspace_dump2console(id);
}
