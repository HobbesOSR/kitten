/* Copyright (c) 2007, Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/log2.h>
#include <lwk/buddy.h>
#include <lwk/bootmem.h>


/**
 * Each free block has one of these structures at its head. The link member
 * provides linkage for the mp->avail[order] free list, where order is the
 * size of the free block.
 */
struct block {
	struct list_head link;
	unsigned long    order;
};


/**
 * Converts a block address to its block index in the specified buddy allocator.
 * A block's index is used to find the block's tag bit, mp->tag_bits[block_id].
 */
static unsigned long
block_to_id(struct buddy_mempool *mp, struct block *block)
{
	unsigned long block_id =
		((unsigned long)block - mp->base_addr) >> mp->min_order;
	BUG_ON(block_id >= mp->num_blocks);
	return block_id;
}


/**
 * Marks a block as free by setting its tag bit to one.
 */
static void
mark_available(struct buddy_mempool *mp, struct block *block)
{
	__set_bit(block_to_id(mp, block), mp->tag_bits);
}


/**
 * Marks a block as allocated by setting its tag bit to zero.
 */
static void
mark_allocated(struct buddy_mempool *mp, struct block *block)
{
	__clear_bit(block_to_id(mp, block), mp->tag_bits);
}


/**
 * Returns true if block is free, false if it is allocated.
 */
static int
is_available(struct buddy_mempool *mp, struct block *block)
{
	return test_bit(block_to_id(mp, block), mp->tag_bits);
}


/**
 * Returns the address of the block's buddy block.
 */
static void *
find_buddy(struct buddy_mempool *mp, struct block *block, unsigned long order)
{
	unsigned long _block;
	unsigned long _buddy;

	BUG_ON((unsigned long)block < mp->base_addr);

	/* Fixup block address to be zero-relative */
	_block = (unsigned long)block - mp->base_addr;

	/* Calculate buddy in zero-relative space */
	_buddy = _block ^ (1UL << order);

	/* Return the buddy's address */
	return (void *)(_buddy + mp->base_addr);
}


/**
 * Initializes a buddy system memory allocator object.
 *
 * Arguments:
 *       [IN] base_addr:  Base address of the memory pool.
 *       [IN] pool_order: Size of the memory pool (2^pool_order bytes).
 *       [IN] min_order:  Minimum allocatable block size (2^min_order bytes).
 *
 * Returns:
 *       Success: Pointer to an initialized buddy system memory allocator.
 *       Failure: NULL
 *
 * NOTE: The min_order argument is provided as an optimization. Since one tag
 *       bit is required for each minimum-sized block, large memory pools that
 *       allow order 0 allocations will use large amounts of memory. Specifying
 *       a min_order of 5 (32 bytes), for example, reduces the number of tag
 *       bits by 32x.
 */
struct buddy_mempool *
buddy_init(
	unsigned long base_addr,
	unsigned long pool_order,
	unsigned long min_order
)
{
	struct buddy_mempool *mp;
	unsigned long i;

	/* Smallest block size must be big enough to hold a block structure */
	if ((1UL << min_order) < sizeof(struct block))
		min_order = ilog2( roundup_pow_of_two(sizeof(struct block)) );

	/* The minimum block order must be smaller than the pool order */
	if (min_order > pool_order)
		return NULL;

	mp = alloc_bootmem(sizeof(struct buddy_mempool));
	
	mp->base_addr  = base_addr;
	mp->pool_order = pool_order;
	mp->min_order  = min_order;

	/* Allocate a list for every order up to the maximum allowed order */
	mp->avail = alloc_bootmem((pool_order + 1) * sizeof(struct list_head));

	/* Initially all lists are empty */
	for (i = 0; i <= pool_order; i++)
		INIT_LIST_HEAD(&mp->avail[i]);

	/* Allocate a bitmap with 1 bit per minimum-sized block */
	mp->num_blocks = (1UL << pool_order) / (1UL << min_order);
	mp->tag_bits   = alloc_bootmem(
	                     BITS_TO_LONGS(mp->num_blocks) * sizeof(long)
	                 );

	/* Initially mark all minimum-sized blocks as allocated */
	bitmap_zero(mp->tag_bits, mp->num_blocks);

	return mp;
}


/**
 * Allocates a block of memory of the requested size (2^order bytes).
 *
 * Arguments:
 *       [IN] mp:    Buddy system memory allocator object.
 *       [IN] order: Block size to allocate (2^order bytes).
 *
 * Returns:
 *       Success: Pointer to the start of the allocated memory block.
 *       Failure: NULL
 */
void *
buddy_alloc(struct buddy_mempool *mp, unsigned long order)
{
	unsigned long j;
	struct list_head *list;
	struct block *block;
	struct block *buddy_block;

	BUG_ON(mp == NULL);
	BUG_ON(order > mp->pool_order);

	/* Fixup requested order to be at least the minimum supported */
	if (order < mp->min_order)
		order = mp->min_order;

	for (j = order; j <= mp->pool_order; j++) {

		/* Try to allocate the first block in the order j list */
		list = &mp->avail[j];
		if (list_empty(list))
			continue;
		block = list_entry(list->next, struct block, link);
		list_del(&block->link);
		mark_allocated(mp, block);

		/* Trim if a higher order block than necessary was allocated */
		while (j > order) {
			--j;
			buddy_block = (struct block *)((unsigned long)block + (1UL << j));
			buddy_block->order = j;
			mark_available(mp, buddy_block);
			list_add(&buddy_block->link, &mp->avail[j]);
		}

		return block;
	}

	return NULL;
}


/**
 * Returns a block of memory to the buddy system memory allocator.
 */
void
buddy_free(
	//!    Buddy system memory allocator object.
	struct buddy_mempool *	mp,
	//!  Address of memory block to free.
	const void *		addr,
	//! Size of the memory block (2^order bytes).
	unsigned long		order
)
{
	BUG_ON(mp == NULL);
	BUG_ON(order > mp->pool_order);

	/* Fixup requested order to be at least the minimum supported */
	if (order < mp->min_order)
		order = mp->min_order;

	/* Overlay block structure on the memory block being freed */
	struct block * block = (struct block *) addr;
	BUG_ON(is_available(mp, block));

	/* Coalesce as much as possible with adjacent free buddy blocks */
	while (order < mp->pool_order) {
		/* Determine our buddy block's address */
		struct block * buddy = find_buddy(mp, block, order);

		/* Make sure buddy is available and has the same size as us */
		if (!is_available(mp, buddy))
			break;
		if (is_available(mp, buddy) && (buddy->order != order))
			break;

		/* OK, we're good to go... buddy merge! */
		list_del(&buddy->link);
		if (buddy < block)
			block = buddy;
		++order;
		block->order = order;
	}

	/* Add the (possibly coalesced) block to the appropriate free list */
	block->order = order;
	mark_available(mp, block);
	list_add(&block->link, &mp->avail[order]);
}


/**
 * Dumps the state of a buddy system memory allocator object to the console.
 */
void
buddy_dump_mempool(struct buddy_mempool *mp)
{
	unsigned long i;
	unsigned long num_blocks;
	struct list_head *entry;

	printk(KERN_DEBUG "DUMP OF BUDDY MEMORY POOL:\n");

	for (i = mp->min_order; i <= mp->pool_order; i++) {

		/* Count the number of memory blocks in the list */
		num_blocks = 0;
		list_for_each(entry, &mp->avail[i])
			++num_blocks;

		printk(KERN_DEBUG "  order %2lu: %lu free blocks\n", i, num_blocks);
	}
}


