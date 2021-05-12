/*
 * Procedures for maintaining information about logical memory blocks.
 *
 * Peter Bergner, IBM Corp.	June 2001.
 * Copyright (C) 2001 Peter Bergner.
 *
 *      This program is free software; you can redistribute it and/or
 *      modify it under the terms of the GNU General Public License
 *      as published by the Free Software Foundation; either version
 *      2 of the License, or (at your option) any later version.
 */

#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/bitops.h>
#include <lwk/pfn.h>
#include <arch/memblock.h>

#define printk early_printk

static struct memblock_region memblock_memory_init_regions[INIT_MEMBLOCK_REGIONS] __initdata_memblock;
static struct memblock_region memblock_reserved_init_regions[INIT_MEMBLOCK_REGIONS] __initdata_memblock;

struct memblock memblock __initdata_memblock = {
	.memory.regions		= memblock_memory_init_regions,
	.memory.cnt		= 1,	/* empty dummy entry */
	.memory.max		= INIT_MEMBLOCK_REGIONS,

	.reserved.regions	= memblock_reserved_init_regions,
	.reserved.cnt		= 1,	/* empty dummy entry */
	.reserved.max		= INIT_MEMBLOCK_REGIONS,

	.current_limit		= MEMBLOCK_ALLOC_ANYWHERE,
};


/* inline so we don't get a warning when pr_debug is compiled out */
static __init_memblock const char *
memblock_type_name(struct memblock_type *type)
{
	return ((type == &memblock.memory)   ? "memory"   : 
	   	(type == &memblock.reserved) ? "reserved" : "unknown");
}


/*
 * Address comparison utilities
 */
static unsigned long __init_memblock 
memblock_addrs_overlap(phys_addr_t base1, 
		       phys_addr_t size1,
		       phys_addr_t base2, 
		       phys_addr_t size2)
{
	return ((base1 < (base2 + size2)) && (base2 < (base1 + size1)));
}

static long __init_memblock 
memblock_overlaps_region(struct memblock_type * type,
			 phys_addr_t            base, 
			 phys_addr_t            size)
{
	unsigned long i;

	for (i = 0; i < type->cnt; i++) {
		phys_addr_t rgnbase = type->regions[i].base;
		phys_addr_t rgnsize = type->regions[i].size;

		if (memblock_addrs_overlap(base, size, rgnbase, rgnsize)) {
			break;
		}
	}

	return (i < type->cnt) ? i : -1;
}

/**
 * memblock_find_in_range_node - find free area in given range and node
 * @start: start of candidate range
 * @end:   end of candidate range, can be %MEMBLOCK_ALLOC_{ANYWHERE|ACCESSIBLE}
 * @size:  size of free area to find
 * @align: alignment of free area to find
 * @nid:   nid of the free area to find, %MAX_NUMNODES for any node
 *
 * Find @size free area aligned to @align in the specified range and node.
 *
 * RETURNS:
 * Found address on success, %0 on failure.
 */

// TODO: Move this define to another location
/**
 * clamp - return a value clamped to a given range with strict typechecking
 * @val: current value
 * @min: minimum allowable value
 * @max: maximum allowable value
 *
 * This macro does strict typechecking of min/max to make sure they are of the
 * same type as val.  See the unnecessary pointer comparisons.
 */
#define clamp(val, min, max) ({         \
    typeof(val) __val = (val);      \
    typeof(min) __min = (min);      \
    typeof(max) __max = (max);      \
    (void) (&__val == &__min);      \
    (void) (&__val == &__max);      \
    __val = __val < __min ? __min: __val;   \
    __val > __max ? __max: __val; })

phys_addr_t __init_memblock 
memblock_find_in_range_node(phys_addr_t start,
			    phys_addr_t end, 
			    phys_addr_t size,
			    phys_addr_t align, 
			    int         nid)
{
	phys_addr_t this_start;
	phys_addr_t this_end;
	phys_addr_t cand;
	u64 i;

	/* pump up @end */
	if (end == MEMBLOCK_ALLOC_ACCESSIBLE) {
		end = memblock.current_limit;
	}

	/* avoid allocating the first page */
	start = max_t(phys_addr_t, start, PAGE_SIZE);
	end   = max(start, end);

	for_each_free_mem_range_reverse(i, nid, &this_start, &this_end, NULL) {
		this_start = clamp(this_start, start, end);
		this_end   = clamp(this_end,   start, end);

		if (this_end < size) {
			continue;
		}

		cand = round_down(this_end - size, align);

		if (cand >= this_start) {
			return cand;
		}
	}
	return 0;
}

/**
 * memblock_find_in_range - find free area in given range
 * @start: start of candidate range
 * @end: end of candidate range, can be %MEMBLOCK_ALLOC_{ANYWHERE|ACCESSIBLE}
 * @size: size of free area to find
 * @align: alignment of free area to find
 *
 * Find @size free area aligned to @align in the specified range.
 *
 * RETURNS:
 * Found address on success, %0 on failure.
 */
phys_addr_t __init_memblock 
memblock_find_in_range(phys_addr_t start,
		       phys_addr_t end, 
		       phys_addr_t size,
		       phys_addr_t align)
{
	return memblock_find_in_range_node(start, end, size, align, MAX_NUMNODES);
}



/**
 * memblock_merge_regions - merge neighboring compatible regions
 * @type: memblock type to scan
 *
 * Scan @type and merge neighboring compatible regions.
 */
static void __init_memblock 
memblock_merge_regions(struct memblock_type * type)
{
	int i = 0;

	/* cnt never goes below 1 */
	while (i < type->cnt - 1) {
		struct memblock_region * this = &type->regions[i];
		struct memblock_region * next = &type->regions[i + 1];

		if ((this->base + this->size != next->base) ||
		    (this->nid               != next->nid)) {
			BUG_ON(this->base + this->size > next->base);
			i++;
			continue;
		}

		this->size += next->size;
		memmove(next, next + 1, (type->cnt - (i + 1)) * sizeof(*next));
		type->cnt--;
	}
}

/**
 * memblock_insert_region - insert new memblock region
 * @type: memblock type to insert into
 * @idx: index for the insertion point
 * @base: base address of the new region
 * @size: size of the new region
 *
 * Insert new memblock region [@base,@base+@size) into @type at @idx.
 * @type must already have extra room to accomodate the new region.
 */
static void __init_memblock 
memblock_insert_region(struct memblock_type * type,
		       int                    idx, 
		       phys_addr_t            base,
		       phys_addr_t            size, 
		       int                    nid)
{
	struct memblock_region *rgn = &type->regions[idx];

	BUG_ON(type->cnt >= type->max);

	memmove(rgn + 1, rgn, (type->cnt - idx) * sizeof(*rgn));

	rgn->base = base;
	rgn->size = size;
	rgn->nid  = nid;

	type->cnt        += 1;
	type->total_size += size;
}

/**
 * memblock_add_region - add new memblock region
 * @type: memblock type to add new region into
 * @base: base address of the new region
 * @size: size of the new region
 * @nid: nid of the new region
 *
 * Add new memblock region [@base,@base+@size) into @type.  The new region
 * is allowed to overlap with existing ones - overlaps don't affect already
 * existing regions.  @type is guaranteed to be minimal (all neighbouring
 * compatible regions are merged) after the addition.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
static int __init_memblock 
memblock_add_region(struct memblock_type * type,
		    phys_addr_t            base, 
		    phys_addr_t            size, 
		    int                    nid)
{
	phys_addr_t obase = base;
	phys_addr_t end   = base + size;
	int nr_new;
	int i;

	if (size == 0) {
		return 0;
	}

	/* special case for empty array */
	if (type->regions[0].size == 0) {
		WARN_ON(type->cnt != 1 || type->total_size);

		type->regions[0].base = base;
		type->regions[0].size = size;
		type->regions[0].nid  = nid;

		type->total_size = size;

		return 0;
	}


	base   = obase;
	nr_new = 0;

	for (i = 0; i < type->cnt; i++) {
		struct memblock_region * rgn   = &type->regions[i];
		phys_addr_t              rbase = rgn->base;
		phys_addr_t              rend  = rbase + rgn->size;

		if (rbase >= end) {
			break;
		}

		if (rend <= base) {
			continue;
		}

		/*
		 * @rgn overlaps.  If it separates the lower part of new
		 * area, insert that portion.
		 */
		if (rbase > base) {
			nr_new++;

			memblock_insert_region(type, i++, base,
					       rbase - base, nid);
		}
		/* area below @rend is dealt with, forget about it */
		base = min(rend, end);
	}

	/* insert the remaining portion */
	if (base < end) {
		nr_new++;
		memblock_insert_region(type, i, base, end - base, nid);
	}

	memblock_merge_regions(type);
	return 0;

}

int __init_memblock 
memblock_add_node(phys_addr_t base, 
		  phys_addr_t size,
		  int         nid)
{
	return memblock_add_region(&memblock.memory, base, size, nid);
}

int __init_memblock 
memblock_add(phys_addr_t base, 
	     phys_addr_t size)
{
	return memblock_add_region(&memblock.memory, base, size, MAX_NUMNODES);
}

/**
 * memblock_isolate_range - isolate given range into disjoint memblocks
 * @type: memblock type to isolate range for
 * @base: base of range to isolate
 * @size: size of range to isolate
 * @start_rgn: out parameter for the start of isolated region
 * @end_rgn: out parameter for the end of isolated region
 *
 * Walk @type and ensure that regions don't cross the boundaries defined by
 * [@base,@base+@size).  Crossing regions are split at the boundaries,
 * which may create at most two more regions.  The index of the first
 * region inside the range is returned in *@start_rgn and end in *@end_rgn.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
static int __init_memblock 
memblock_isolate_range(struct memblock_type * type,
                       phys_addr_t            base, 
		       phys_addr_t            size,
		       int                  * start_rgn,
		       int                  * end_rgn)
{
	phys_addr_t end = base + size;

	int i;

	*start_rgn = 0;
	*end_rgn   = 0;

	if (!size) {
		return 0;
	}

	for (i = 0; i < type->cnt; i++)	{
		struct memblock_region * rgn   = &type->regions[i];
		phys_addr_t              rbase = rgn->base;
		phys_addr_t              rend  = rbase + rgn->size;
		
		if (rbase >= end) {
			break;
		}

		if (rend <= base) {
			continue;
		}

		if (rbase < base) {
			/*
                         * @rgn intersects from below.  Split and continue
                         * to process the next region - the new top half.
                         */
			rgn->base         = base;
			rgn->size        -= base - rbase;
			type->total_size -= base - rbase;
			memblock_insert_region(type, i, rbase, base - rbase, rgn->nid);
			
		} else if (rend > end) {
			/*
                         * @rgn intersects from above.  Split and redo the
                         * current region - the new bottom half.
                         */
			rgn->base         = end;
			rgn->size        -= end - rbase;
			type->total_size -= end - rbase;
			memblock_insert_region(type, i--, rbase, end - rbase, rgn->nid);
			
		} else {
			/* @rgn is fully contained, record it */
			if (!*end_rgn) {
				*start_rgn = i;
			}

			*end_rgn = i + 1;
			
		}
	}

	return 0;
}

int __init_memblock 
memblock_reserve(phys_addr_t base, 
		 phys_addr_t size)
{
	struct memblock_type *_rgn = &memblock.reserved;

	printk("memblock_reserve: %p [%d bytes]\n", base, size);

	return memblock_add_region(_rgn, base, size, MAX_NUMNODES);
}

/**
 * __next_free_mem_range - next function for for_each_free_mem_range()
 * @idx: pointer to u64 loop variable
 * @nid: nid: node selector, %MAX_NUMNODES for all nodes
 * @out_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @out_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @out_nid: ptr to int for nid of the range, can be %NULL
 *
 * Find the first free area from *@idx which matches @nid, fill the out
 * parameters, and update *@idx for the next iteration.  The lower 32bit of
 * *@idx contains index into memory region and the upper 32bit indexes the
 * areas before each reserved region.  For example, if reserved regions
 * look like the following,
 *
 *	0:[0-16), 1:[32-48), 2:[128-130)
 *
 * The upper 32bit indexes the following regions.
 *
 *	0:[0-0), 1:[16-32), 2:[48-128), 3:[130-MAX)
 *
 * As both region arrays are sorted, the function advances the two indices
 * in lockstep and returns each intersection.
 */
void __init_memblock 
__next_free_mem_range(u64         * idx, 
		      int           nid,
		      phys_addr_t * out_start,
		      phys_addr_t * out_end, 
		      int         * out_nid)
{
	struct memblock_type * mem = &memblock.memory;
	struct memblock_type * rsv = &memblock.reserved;
	int mi = *idx  & 0xffffffff;
	int ri = *idx >> 32;

	for ( ; mi < mem->cnt; mi++) {
		struct memblock_region * m       = &mem->regions[mi];
		phys_addr_t              m_start = m->base;
		phys_addr_t              m_end   = m->base + m->size;

		/* only memory regions are associated with nodes, check it */
		if ((nid != MAX_NUMNODES) && 
		    (nid != m->nid)) {
			continue;
		}

		/* scan areas before each reservation for intersection */
		for ( ; ri < rsv->cnt + 1; ri++) {
			struct memblock_region * r       = &rsv->regions[ri];
			phys_addr_t              r_start = ri            ? r[-1].base + r[-1].size : 0;
			phys_addr_t              r_end   = ri < rsv->cnt ? r->base                 : ULLONG_MAX;

			/* if ri advanced past mi, break out to advance mi */
			if (r_start >= m_end)
				break;
			/* if the two regions intersect, we're done */
			if (m_start < r_end) {
				if (out_start) *out_start = max(m_start, r_start);
				if (out_end)   *out_end   = min(m_end, r_end);
				if (out_nid)   *out_nid   = m->nid;
				/*
				 * The region which ends first is advanced
				 * for the next iteration.
				 */
				if (m_end <= r_end) {
					mi++;
				} else {
					ri++;
				}

				*idx = (u32)mi | (u64)ri << 32;
				
				return;
			}
		}
	}

	/* signal end of iteration */
	*idx = ULLONG_MAX;
}

/**
 * __next_free_mem_range_rev - next function for for_each_free_mem_range_reverse()
 * @idx: pointer to u64 loop variable
 * @nid: nid: node selector, %MAX_NUMNODES for all nodes
 * @out_start: ptr to phys_addr_t for start address of the range, can be %NULL
 * @out_end: ptr to phys_addr_t for end address of the range, can be %NULL
 * @out_nid: ptr to int for nid of the range, can be %NULL
 *
 * Reverse of __next_free_mem_range().
 */
void __init_memblock 
__next_free_mem_range_rev(u64         * idx, 
			  int           nid,
			  phys_addr_t * out_start,
			  phys_addr_t * out_end, 
			  int         * out_nid)
{
	struct memblock_type * mem = &memblock.memory;
	struct memblock_type * rsv = &memblock.reserved;

	int mi = *idx  & 0xffffffff;
	int ri = *idx >> 32;

	if (*idx == (u64)ULLONG_MAX) {
		mi = mem->cnt - 1;
		ri = rsv->cnt;
	}

	for ( ; mi >= 0; mi--) {
		struct memblock_region * m       = &mem->regions[mi];
		phys_addr_t              m_start = m->base;
		phys_addr_t              m_end   = m->base + m->size;

		/* only memory regions are associated with nodes, check it */
		if ( (nid != MAX_NUMNODES) && 
		     (nid != m->nid)) {
			continue;
		}

		/* scan areas before each reservation for intersection */
		for ( ; ri >= 0; ri--) {
			struct memblock_region * r       = &rsv->regions[ri];
			phys_addr_t              r_start = ri            ? r[-1].base + r[-1].size : 0;
			phys_addr_t              r_end   = ri < rsv->cnt ? r->base                 : ULLONG_MAX;

			/* if ri advanced past mi, break out to advance mi */
			if (r_end <= m_start) {
				break;
			}

			/* if the two regions intersect, we're done */
			if (m_end > r_start) {
				if (out_start) *out_start = max(m_start, r_start);
				if (out_end)   *out_end   = min(m_end, r_end);
				if (out_nid)   *out_nid   = m->nid;

				if (m_start >= r_start) {
					mi--;
				} else {
					ri--;
				}

				*idx = (u32)mi | (u64)ri << 32;
				
				return;
			}
		}
	}

	*idx = ULLONG_MAX;
}


/*
 * Common iterator interface used to define for_each_mem_range().
 */
void __init_memblock 
__next_mem_pfn_range(int           * idx, 
		     int             nid,
		     unsigned long * out_start_pfn,
		     unsigned long * out_end_pfn, 
		     int           * out_nid)
{
	struct memblock_type   * type = &memblock.memory;
	struct memblock_region * r;

	while (++*idx < type->cnt) {
		r = &type->regions[*idx];

		if (PFN_UP(r->base) >= PFN_DOWN(r->base + r->size)) {
			continue;
		}

		if (nid == MAX_NUMNODES || nid == r->nid) {
			break;
		}
	}

	if (*idx >= type->cnt) {
		*idx = -1;
		return;
	}

	if (out_start_pfn) *out_start_pfn = PFN_UP(r->base);
	if (out_end_pfn)   *out_end_pfn   = PFN_DOWN(r->base + r->size);
	if (out_nid)       *out_nid       = r->nid;
}

/**
 * memblock_set_node - set node ID on memblock regions
 * @base: base of area to set node ID for
 * @size: size of area to set node ID for
 * @nid: node ID to set
 *
 * Set the nid of memblock memory regions in [@base,@base+@size) to @nid.
 * Regions which cross the area boundaries are split as necessary.
 *
 * RETURNS:
 * 0 on success, -errno on failure.
 */
int __init_memblock 
memblock_set_node(phys_addr_t base, 
		  phys_addr_t size,
		  int         nid)
{
	struct memblock_type * type = &memblock.memory;

	int start_rgn = 0;
	int end_rgn   = 0;
	int ret       = 0;
	int i         = 0;

	ret = memblock_isolate_range(type, base, size, &start_rgn, &end_rgn);
	if (ret) {
		return ret;
	}

	for (i = start_rgn; i < end_rgn; i++) {
		type->regions[i].nid = nid;
	}

	memblock_merge_regions(type);
	return 0;
}


static phys_addr_t __init 
memblock_alloc_base_nid(phys_addr_t size,
			phys_addr_t align, 
			phys_addr_t max_addr,
			int         nid)
{
	phys_addr_t found;

	/* align @size to avoid excessive fragmentation on reserved array */
	size = round_up(size, align);

	found = memblock_find_in_range_node(0, max_addr, size, align, nid);

	if (found && !memblock_reserve(found, size)) {
		return found;
	}

	return 0;
}

phys_addr_t __init 
memblock_alloc_nid(phys_addr_t size, 
		   phys_addr_t align, 
		   int         nid)
{
	return memblock_alloc_base_nid(size, align, MEMBLOCK_ALLOC_ACCESSIBLE, nid);
}

phys_addr_t __init 
__memblock_alloc_base(phys_addr_t size, 
		      phys_addr_t align, 
		      phys_addr_t max_addr)
{
	return memblock_alloc_base_nid(size, align, max_addr, MAX_NUMNODES);
}

phys_addr_t __init 
memblock_alloc_base(phys_addr_t size, 
		    phys_addr_t align, 
		    phys_addr_t max_addr)
{
	phys_addr_t alloc;

	alloc = __memblock_alloc_base(size, align, max_addr);

	if (alloc == 0)
		panic("ERROR: Failed to allocate 0x%llx bytes below 0x%llx.\n",
		      (unsigned long long) size, (unsigned long long) max_addr);

	return alloc;
}

phys_addr_t __init 
memblock_alloc(phys_addr_t size, 
	       phys_addr_t align)
{
	return memblock_alloc_base(size, align, MEMBLOCK_ALLOC_ACCESSIBLE);
}

/*
 * Remaining API functions
 */

phys_addr_t __init 
memblock_phys_mem_size(void)
{
	return memblock.memory.total_size;
}

/* lowest address */
phys_addr_t __init_memblock 
memblock_start_of_DRAM(void)
{
	return memblock.memory.regions[0].base;
}

phys_addr_t __init_memblock 
memblock_end_of_DRAM(void)
{
	int idx = memblock.memory.cnt - 1;

	return (memblock.memory.regions[idx].base + memblock.memory.regions[idx].size);
}



static int __init_memblock 
memblock_search(struct memblock_type * type, 
		phys_addr_t            addr)
{
	unsigned int left  = 0;
	unsigned int right = type->cnt;

	do {
		unsigned int mid = (right + left) / 2;

		if (addr < type->regions[mid].base) {
			right = mid;
		} else if (addr >= (type->regions[mid].base + type->regions[mid].size)) {
			left  = mid + 1;
		} else {
			return mid;
		}
	} while (left < right);

	return -1;
}

int __init memblock_is_reserved(phys_addr_t addr)
{
	return memblock_search(&memblock.reserved, addr) != -1;
}

int __init_memblock memblock_is_memory(phys_addr_t addr)
{
	return memblock_search(&memblock.memory, addr) != -1;
}

/**
 * memblock_is_region_memory - check if a region is a subset of memory
 * @base: base of region to check
 * @size: size of region to check
 *
 * Check if the region [@base, @base+@size) is a subset of a memory block.
 *
 * RETURNS:
 * 0 if false, non-zero if true
 */
int __init_memblock 
memblock_is_region_memory(phys_addr_t base, 
			  phys_addr_t size)
{
	int idx = memblock_search(&memblock.memory, base);

	phys_addr_t end = base + size;

	if (idx == -1) {
		return 0;
	}

	return (( (memblock.memory.regions[idx].base)                                     <= base ) &&
		( (memblock.memory.regions[idx].base + memblock.memory.regions[idx].size) >= end  ));
}

/**
 * memblock_is_region_reserved - check if a region intersects reserved memory
 * @base: base of region to check
 * @size: size of region to check
 *
 * Check if the region [@base, @base+@size) intersects a reserved memory block.
 *
 * RETURNS:
 * 0 if false, non-zero if true
 */
int __init_memblock 
memblock_is_region_reserved(phys_addr_t base, 
			    phys_addr_t size)
{
	return memblock_overlaps_region(&memblock.reserved, base, size) >= 0;
}



void __init_memblock 
memblock_set_current_limit(phys_addr_t limit)
{
	memblock.current_limit = limit;
}

static void __init_memblock 
memblock_dump(struct memblock_type * type,
	      char                 * name)
{
	unsigned long long base;
	unsigned long long size;
	int i;

	printk(" [%s] cnt  = %d\n", name, type->cnt);

	for (i = 0; i < type->cnt; i++) {
		struct memblock_region *rgn = &type->regions[i];
		char nid_buf[32] = "";

		base = rgn->base;
		size = rgn->size;

		if (rgn->nid != MAX_NUMNODES) {
			snprintf(nid_buf, sizeof(nid_buf), " on node %d", rgn->nid);
		}

		printk(" \t[%d]\t[%p-%p], %lld bytes %s\n",
			 i, base, base + size - 1, size, nid_buf);
	}
}

void __init_memblock memblock_dump_all(void)
{
	printk("MEMBLOCK configuration:\n");
	printk(" memory size = %lld reserved size = %lld\n",
		(unsigned long long)memblock.memory.total_size,
		(unsigned long long)memblock.reserved.total_size);

	memblock_dump(&memblock.memory,   "memory");
	memblock_dump(&memblock.reserved, "reserved");
}



