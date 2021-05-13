/*
 *  lwk/mm/bootmem.c
 *
 *  Copyright (C) 1999 Ingo Molnar
 *  Discontiguous memory support, Kanoj Sarcar, SGI, Nov 1999
 *
 *  simple boot-time physical memory area allocator and
 *  free memory collector. It's used to deal with reserved
 *  system memory and memory holes as well.
 */

#include <lwk/init.h>
#include <lwk/pfn.h>
#include <lwk/bootmem.h>
#include <lwk/params.h>
#include <lwk/log2.h>
#include <lwk/pmem.h>
#include <lwk/kmem.h>
#include <lwk/bitops.h>
#include <lwk/acpi.h>
#include <arch/io.h>
#include <arch/memory.h>

/**
 * Set to true once bootmem allocator has been destroyed.
 */
bool bootmem_destoyed = false;

/**
 * Access to this subsystem has to be serialized externally.
 * (this is true for the boot process anyway)
 */


/**
 * Amount of system memory to reserve for use by the kernel. The first
 * kmem_size bytes of system memory [0, kmem_size) will be added to the
 * kernel memory pool. The remainder of system memory is left untouched by
 * the kernel and is available for use by applications.
 */
static unsigned long kmem_size = (1024 * 1024 * 64);  /* default is first 64 MB */
param(kmem_size, ulong);


/**
 *
 */
static bootmem_data_t __initdata bootmem_data;

/**
 * List of bootmem_data structures, each describing a section of
 * physical memory.
 */
static LIST_HEAD(bdata_list);

/**
 * Returns the number of _pages_ that will be allocated for the boot bitmap.
 */
unsigned long __init
bootmem_bootmap_pages(unsigned long pages)
{
	unsigned long mapsize;

	mapsize = (pages+7)/8;
	mapsize = (mapsize + ~PAGE_MASK) & PAGE_MASK;
	mapsize >>= PAGE_SHIFT;

	return mapsize;
}

/**
 * Links a newly created bootmem_data structure to the bdata_list.
 */
static void __init
link_bootmem(bootmem_data_t *bdata)
{
	bootmem_data_t *ent;
	if (list_empty(&bdata_list)) {
		list_add(&bdata->list, &bdata_list);
		return;
	}
	/* insert in order */
	list_for_each_entry(ent, &bdata_list, list) {
		if (bdata->node_boot_start < ent->node_boot_start) {
			list_add_tail(&bdata->list, &ent->list);
			return;
		}
	}
	list_add_tail(&bdata->list, &bdata_list);
	return;
}

/**
 * Called once to set up the allocator itself.
 */
static unsigned long __init
init_bootmem_core(
	bootmem_data_t	*bdata,
	unsigned long	mapstart_pfn,
	unsigned long	start_pfn,
	unsigned long	end_pfn
)
{
	unsigned long mapsize = ((end_pfn - start_pfn) + 7) / 8;

	mapsize = ALIGN(mapsize, sizeof(long));
	bdata->node_bootmem_map = phys_to_virt(mapstart_pfn << PAGE_SHIFT);
	bdata->node_boot_start = (start_pfn << PAGE_SHIFT);
	bdata->node_low_pfn = end_pfn;
	link_bootmem(bdata);

	/*
	 * Initially all pages are reserved - setup_arch() has to
	 * register free RAM areas explicitly.
	 */
	memset(bdata->node_bootmem_map, 0xff, mapsize);

	return mapsize;
}

/**
 * Marks a particular physical memory range as unallocatable. Usable RAM
 * might be used for boot-time allocations - or it might get added
 * to the free page pool later on.
 */
static void __init
reserve_bootmem_core(
	bootmem_data_t	*bdata,
	unsigned long	addr,
	unsigned long	size
)
{
	unsigned long sidx, eidx;
	unsigned long i;

	/*
	 * round up, partially reserved pages are considered
	 * fully reserved.
	 */
	BUG_ON(!size);
	BUG_ON(PFN_DOWN(addr) >= bdata->node_low_pfn);
	BUG_ON(PFN_UP(addr + size) > bdata->node_low_pfn);

	sidx = PFN_DOWN(addr - bdata->node_boot_start);
	eidx = PFN_UP(addr + size - bdata->node_boot_start);

	for (i = sidx; i < eidx; i++) {
		if (test_and_set_bit(i, bdata->node_bootmem_map)) {
#ifdef CONFIG_DEBUG_BOOTMEM
			printk("hm, page %08lx reserved twice.\n", i*PAGE_SIZE);
#endif
		}
	}
}

/**
 * Frees a section of bootmemory.
 */
static void __init
free_bootmem_core(
	bootmem_data_t	*bdata,
	unsigned long	addr,
	unsigned long	size
)
{
	unsigned long i;
	unsigned long start;
	/*
	 * round down end of usable mem, partially free pages are
	 * considered reserved.
	 */
	unsigned long sidx;
	unsigned long eidx = (addr + size - bdata->node_boot_start)/PAGE_SIZE;
	unsigned long end = (addr + size)/PAGE_SIZE;

	BUG_ON(!size);
	BUG_ON(end > bdata->node_low_pfn);

	if (addr < bdata->last_success)
		bdata->last_success = addr;

	/*
	 * Round up the beginning of the address.
	 */
	start = (addr + PAGE_SIZE-1) / PAGE_SIZE;
	sidx = start - (bdata->node_boot_start/PAGE_SIZE);

	for (i = sidx; i < eidx; i++) {
		if (unlikely(!test_and_clear_bit(i, bdata->node_bootmem_map)))
			BUG();
	}
}

/**
 * We 'merge' subsequent allocations to save space. We might 'lose'
 * some fraction of a page if allocations cannot be satisfied due to
 * size constraints on boxes where there is physical RAM space
 * fragmentation - in these cases (mostly large memory boxes) this
 * is not a problem.
 *
 * On low memory boxes we get it right in 100% of the cases.
 *
 * alignment has to be a power of 2 value.
 *
 * NOTE:  This function is _not_ reentrant.
 */
void * __init
__alloc_bootmem_core(
	struct bootmem_data	*bdata,
	unsigned long		size,
	unsigned long		align,
	unsigned long		goal,
	unsigned long		limit
)
{
	unsigned long offset, remaining_size, areasize, preferred;
	unsigned long i, start = 0, incr, eidx, end_pfn = bdata->node_low_pfn;
	void *ret;

	if (bootmem_destoyed)
		panic("The bootmem allocator has been destroyed.");

	if(!size) {
		printk("__alloc_bootmem_core(): zero-sized request\n");
		BUG();
	}
	BUG_ON(align & (align-1));

	if (limit && bdata->node_boot_start >= limit)
		return NULL;

        limit >>=PAGE_SHIFT;
	if (limit && end_pfn > limit)
		end_pfn = limit;

	eidx = end_pfn - (bdata->node_boot_start >> PAGE_SHIFT);
	offset = 0;
	if (align &&
	    (bdata->node_boot_start & (align - 1UL)) != 0)
		offset = (align - (bdata->node_boot_start & (align - 1UL)));
	offset >>= PAGE_SHIFT;

	/*
	 * We try to allocate bootmem pages above 'goal'
	 * first, then we try to allocate lower pages.
	 */
	if (goal && (goal >= bdata->node_boot_start) && 
	    ((goal >> PAGE_SHIFT) < end_pfn)) {
		preferred = goal - bdata->node_boot_start;

		if (bdata->last_success >= preferred)
			if (!limit || (limit && limit > bdata->last_success))
				preferred = bdata->last_success;
	} else
		preferred = 0;

	preferred = ALIGN(preferred, align) >> PAGE_SHIFT;
	preferred += offset;
	areasize = (size+PAGE_SIZE-1)/PAGE_SIZE;
	incr = align >> PAGE_SHIFT ? : 1;

restart_scan:
	for (i = preferred; i < eidx; i += incr) {
		unsigned long j;
		i = find_next_zero_bit(bdata->node_bootmem_map, eidx, i);
		i = ALIGN(i, incr);
		if (i >= eidx)
			break;
		if (test_bit(i, bdata->node_bootmem_map))
			continue;
		for (j = i + 1; j < i + areasize; ++j) {
			if (j >= eidx)
				goto fail_block;
			if (test_bit (j, bdata->node_bootmem_map))
				goto fail_block;
		}
		start = i;
		goto found;
	fail_block:
		i = ALIGN(j, incr);
	}

	if (preferred > offset) {
		preferred = offset;
		goto restart_scan;
	}
	return NULL;

found:
	bdata->last_success = start << PAGE_SHIFT;
	BUG_ON(start >= eidx);

	/*
	 * Is the next page of the previous allocation-end the start
	 * of this allocation's buffer? If yes then we can 'merge'
	 * the previous partial page with this allocation.
	 */
	if (align < PAGE_SIZE &&
	    bdata->last_offset && bdata->last_pos+1 == start) {
		offset = ALIGN(bdata->last_offset, align);
		BUG_ON(offset > PAGE_SIZE);
		remaining_size = PAGE_SIZE-offset;
		if (size < remaining_size) {
			areasize = 0;
			/* last_pos unchanged */
			bdata->last_offset = offset+size;
			ret = phys_to_virt(bdata->last_pos*PAGE_SIZE + offset +
						bdata->node_boot_start);
		} else {
			remaining_size = size - remaining_size;
			areasize = (remaining_size+PAGE_SIZE-1)/PAGE_SIZE;
			ret = phys_to_virt(bdata->last_pos*PAGE_SIZE + offset +
						bdata->node_boot_start);
			bdata->last_pos = start+areasize-1;
			bdata->last_offset = remaining_size;
		}
		bdata->last_offset &= ~PAGE_MASK;
	} else {
		bdata->last_pos = start + areasize - 1;
		bdata->last_offset = size & ~PAGE_MASK;
		ret = phys_to_virt(start * PAGE_SIZE + bdata->node_boot_start);
	}

	/*
	 * Reserve the area now:
	 */
	for (i = start; i < start+areasize; i++)
		if (unlikely(test_and_set_bit(i, bdata->node_bootmem_map)))
			BUG();
	memset(ret, 0, size);
	return ret;
}

static void __init
free_all_bootmem_core(struct bootmem_data *bdata)
{
	unsigned long pfn;
	unsigned long vaddr;
	unsigned long i, m, count;
	unsigned long bootmem_total=0, kmem_total=0, umem_total=0;
	unsigned long kmem_max_idx, max_idx;
	unsigned long *map; 
	struct pmem_region rgn;

	BUG_ON(!bdata->node_bootmem_map);

	kmem_max_idx = (kmem_size >> PAGE_SHIFT);
	max_idx = bdata->node_low_pfn - (bdata->node_boot_start >> PAGE_SHIFT);
	BUG_ON(kmem_max_idx > max_idx);	/* kmem region doesn't fit into available bootmem */

	/* Create the initial kernel managed memory pool (kmem) */
	count = 0;
	pfn = bdata->node_boot_start >> PAGE_SHIFT;  /* first extant page of node */
	map = bdata->node_bootmem_map;
	for (i = 0; i < kmem_max_idx; ) {
		unsigned long v = ~map[i / BITS_PER_LONG];

		if (v) {
			vaddr = (unsigned long) __va(pfn << PAGE_SHIFT);
			for (m = 1; m && i < kmem_max_idx; m<<=1, vaddr+=PAGE_SIZE, i++) {
				if (v & m) {
					count++;
					kmem_add_memory(vaddr, PAGE_SIZE);
				}
			}
		} else {
			i+=BITS_PER_LONG;
		}
		pfn += BITS_PER_LONG;
	}
	BUG_ON(count == 0);

	/*
	 * At this point, kmem_alloc() will work. The physical memory tracking
	 * code relies on kmem_alloc(), so it cannot be initialized until now.
	 *
	 * Tell the physical memory tracking subsystem about the kernel-managed
	 * pool and the remaining memory that will be managed by user-space.
	 */
	pfn = bdata->node_boot_start >> PAGE_SHIFT;  /* first extant page of node */
	map = bdata->node_bootmem_map;
	pmem_region_unset_all(&rgn);
	rgn.type_is_set = true;
	rgn.allocated_is_set = true;
	rgn.numa_node_is_set = true;
	for (i = 0; i < max_idx; ) {
		unsigned long v = ~map[i / BITS_PER_LONG];
		unsigned long paddr = (unsigned long) pfn << PAGE_SHIFT;

		for (m = 1; m && i < max_idx; m<<=1, paddr+=PAGE_SIZE, i++) {
			rgn.start = paddr;
			rgn.end   = paddr + PAGE_SIZE;

			if (v & m) {
				if (i < kmem_max_idx) {
					rgn.type = PMEM_TYPE_KMEM;
					rgn.allocated = true;
					rgn.numa_node = 0;
					++kmem_total;
				} else {
					rgn.type = PMEM_TYPE_UMEM;
					rgn.allocated = false;
					rgn.numa_node = 0;
					++umem_total;
				}
			} else {
				rgn.type = PMEM_TYPE_BOOTMEM;
				rgn.allocated = true;
				rgn.numa_node = 0;
				++bootmem_total;
			}

			if (pmem_add(&rgn))
				BUG();
		}

		pfn += BITS_PER_LONG;
	}

	/*
	 * Now free the allocator bitmap itself, it's not
	 * needed anymore:
	 */
	vaddr = (unsigned long)bdata->node_bootmem_map;
	count = 0;
	pmem_region_unset_all(&rgn);
	rgn.type_is_set = true;
	rgn.allocated_is_set = true;
	rgn.numa_node_is_set = true;
	for (i = 0; i < ((bdata->node_low_pfn-(bdata->node_boot_start >> PAGE_SHIFT))/8 + PAGE_SIZE-1)/PAGE_SIZE; i++,vaddr+=PAGE_SIZE) {
		count++;

		rgn.start = __pa(vaddr);
		rgn.end   = rgn.start + PAGE_SIZE;

		if (i < kmem_max_idx) {
			kmem_add_memory(vaddr, PAGE_SIZE);
			rgn.type = PMEM_TYPE_KMEM;
			rgn.allocated = true;
			rgn.numa_node = 0;
		} else {
			rgn.type = PMEM_TYPE_UMEM;
			rgn.allocated = false;
			rgn.numa_node = 0;
		}

		pmem_add(&rgn);
	}
	BUG_ON(count == 0);

	/* Mark the bootmem allocator as dead */
	bdata->node_bootmem_map = NULL;

	/*
	 * Figure out which NUMA node each memory region belongs to.
	 * TODO: move this to arch-dependent code
	 */
#ifdef CONFIG_ACPI
	acpi_set_pmem_numa_info();
#endif

	printk(KERN_DEBUG
	       "The boot-strap bootmem allocator has been destroyed:\n");
	printk(KERN_DEBUG
	       "  %lu bytes released to the kernel-managed memory pool (kmem)\n",
	       kmem_total << PAGE_SHIFT);
	printk(KERN_DEBUG
	       "  %lu bytes released to the user-managed memory pool (umem)\n",
	       umem_total << PAGE_SHIFT);
}

/**
 * Initialize boot memory allocator.
 */
unsigned long __init
init_bootmem(unsigned long mapstart_pfn, 
	     unsigned long start_pfn, 
	     unsigned long end_pfn)
{
	return init_bootmem_core(&bootmem_data, mapstart_pfn, start_pfn, end_pfn);
}

/**
 * Reserve a portion of the boot memory.
 * This prevents the reserved memory from being allocated.
 */
void __init
reserve_bootmem(unsigned long addr, unsigned long size)
{
	reserve_bootmem_core(&bootmem_data, addr, size);
}

/**
 * Return a portion of boot memory to the free pool.
 * Note that the region freed is the set of pages covering
 * the byte range [addr, addr+size).
 */
void __init
free_bootmem(unsigned long addr, unsigned long size)
{
	free_bootmem_core(&bootmem_data, addr, size);
}

void __init
free_all_bootmem(void)
{
	free_all_bootmem_core(&bootmem_data);
	bootmem_destoyed = true;
}

static void * __init
__alloc_bootmem_nopanic(unsigned long size, unsigned long align, unsigned long goal)
{
	bootmem_data_t *bdata;
	void *ptr;

	list_for_each_entry(bdata, &bdata_list, list)
		if ((ptr = __alloc_bootmem_core(bdata, size, align, goal, 0)))
			return(ptr);
	return NULL;
}

/**
 * Allocate a chunk of memory from the boot memory allocator.
 *
 *     size  = number of bytes requested
 *     align = required alignment
 *     goal  = hint specifying address to start search.
 */
void * __init
__alloc_bootmem(unsigned long size, unsigned long align, unsigned long goal)
{
	void *mem = __alloc_bootmem_nopanic(size,align,goal);
	if (mem)
		return mem;
	/*
	 * Whoops, we cannot satisfy the allocation request.
	 */
	printk(KERN_ALERT "bootmem alloc of %lu bytes failed!\n", size);
	panic("Out of memory");
	return NULL;
}

/**
 * Allocates a block of memory of the specified size.
 */
void * __init
alloc_bootmem(unsigned long size)
{
	return __alloc_bootmem(size, SMP_CACHE_BYTES, 0);
}

/**
 * Allocates a block of memory of the specified size and alignment.
 */
void * __init
alloc_bootmem_aligned(unsigned long size, unsigned long align)
{
	return __alloc_bootmem(size, align, 0);
}

/**
 * Initializes the kernel memory subsystem.
 */
void __init
mem_subsys_init(void)
{
	/* We like powers of two */
	if (!is_power_of_2(kmem_size)) {
		printk(KERN_WARNING "kmem_size must be a power of two.");
		kmem_size = roundup_pow_of_two(kmem_size);
	}

	printk(KERN_DEBUG
	       "First %lu bytes of system memory reserved for the kernel.\n",
	       kmem_size);

	/* Initialize the kernel memory pool */
	kmem_create_zone((unsigned long)__va( bootmem_data.node_boot_start), kmem_size);
	free_all_bootmem();
	arch_memsys_init(kmem_size);
}

