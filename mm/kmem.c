/* Copyright (c) 2007, Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/buddy.h>
#include <lwk/log2.h>


/**
 * This specifies the minimum sized memory block to request from the underlying
 * buddy system memory allocator, 2^MIN_ORDER bytes. It must be at least big
 * enough to hold a 'struct kmem_block_hdr'.
 */
#define MIN_ORDER	5  /* 32 bytes */


/**
 * Magic value used for sanity checking. Every block of memory allocated via
 * kmem_alloc() has this value in its block header.
 */
#define KMEM_MAGIC	0xF0F0F0F0F0F0F0F0UL


/**
 * The kernel memory pool. This manages all memory available for dynamic
 * allocation by the kernel. The kernel reserves some amount of memory
 * (e.g., the first 8 MB, amount specifiable on kernel boot command line) for
 * its own use, included in which is the kernel memory pool. The rest of memory
 * is reserved for user applications.
 *
 * NOTE: There is currently only one buddy_mempool for the entire system.
 *       This may change to one per NUMA node in the future.
 */
static struct buddy_mempool *kmem = NULL;


/**
 * Total number of bytes in the kernel memory pool.
 */
static unsigned long kmem_bytes_managed;


/**
 * Total number of bytes allocated from the kernel memory pool.
 */
static unsigned long kmem_bytes_allocated;


/**
 * Each block of memory allocated from the kernel memory pool has one of these
 * structures at its head. The structure contains information needed to free
 * the block and return it to the underlying memory allocator.
 *
 * When a block is allocated, the address returned to the caller is
 * sizeof(struct kmem_block_hdr) bytes greater than the block allocated from
 * the underlying memory allocator.
 *
 * WARNING: This structure is defined to be exactly 16 bytes in size.
 *          Do not change this unless you really know what you are doing.
 */
struct kmem_block_hdr {
	uint64_t order;  /* order of the block allocated from buddy system */
	uint64_t magic;  /* magic value used as sanity check */
} __attribute__((packed));


/**
 * This adds a zone to the kernel memory pool. Zones exist to allow there to be
 * multiple non-adjacent regions of physically contiguous memory. The
 * bookkeeping needed to cover the gaps would waste a lot of memory and have no
 * benefit.
 *
 * Arguments:
 *       [IN] base_addr: Base address of the memory pool.
 *       [IN] size:      Size of the memory pool in bytes.
 *
 * NOTE: Currently only one zone is supported. Calling kmem_create_zone() more
 *       than once will result in a panic.
 */
void
kmem_create_zone(unsigned long base_addr, size_t size)
{
	unsigned long pool_order = ilog2(roundup_pow_of_two(size));
	unsigned long min_order  = MIN_ORDER;

	/* For now, protect against calling kmem_create_zone() more than once */
	BUG_ON(kmem != NULL);

	/* Initialize the underlying buddy allocator */
	if ((kmem = buddy_init(base_addr, pool_order, min_order)) == NULL)
		panic("buddy_init() failed.");
}


/**
 * This adds memory to the kernel memory pool. The memory region being added
 * must fall within a zone previously specified via kmem_create_zone().
 *
 * Arguments:
 *       [IN] base_addr: Base address of the memory region to add.
 *       [IN] size:      Size of the memory region in bytes.
 */
void
kmem_add_memory(unsigned long base_addr, size_t size)
{
	/*
	 * kmem buddy allocator is initially empty.
	 * Memory is added to it via buddy_free().
	 * buddy_free() will panic if there are any problems with the args.
	 */
	buddy_free(kmem, (void *)base_addr, ilog2(size));

	/* Update statistics */
	kmem_bytes_managed += size;
}


/**
 * Allocates memory from the kernel memory pool. This will return a memory
 * region that is at least 16-byte aligned.
 *
 * Arguments:
 *       [IN] size: Amount of memory to allocate in bytes.
 *
 * Returns:
 *       Success: Pointer to the start of the allocated memory.
 *       Failure: NULL
 */
void *
kmem_alloc(size_t size)
{
	unsigned long order;
	struct kmem_block_hdr *hdr;

	/* Make room for block header */
	size += sizeof(struct kmem_block_hdr);

	/* Calculate the block order needed */
	order = ilog2(roundup_pow_of_two(size));
	if (order < MIN_ORDER)
		order = MIN_ORDER;

	/* Allocate memory from the underlying buddy system */
	if ((hdr = buddy_alloc(kmem, order)) == NULL)
		return NULL;

	/* Initialize the block header */
	hdr->order = order;       /* kmem_free() needs this to free the block */
	hdr->magic = KMEM_MAGIC;  /* used for sanity check */

	/* Update statistics */
	kmem_bytes_allocated += (1UL << order);

	/* Return address of first byte after block header to caller */
	return hdr + 1;
}


/**
 * Frees memory previously allocated with kmem_alloc().
 *
 * Arguments:
 *       [IN] addr: Address of the memory region to free.
 *
 * NOTE: The size of the memory region being freed is assumed to be in a
 *       'struct kmem_block_hdr' header located immediately before the address
 *       passed in by the caller. This header is created and initialized by
 *       kmem_alloc().
 */
void
kmem_free(void *addr)
{
	struct kmem_block_hdr *hdr;

	BUG_ON((unsigned long)addr < sizeof(struct kmem_block_hdr));

	/* Find the block header */
	hdr = (struct kmem_block_hdr *)addr - 1;
	BUG_ON(hdr->magic != KMEM_MAGIC);

	/* Return block to the underlying buddy system */
	buddy_free(kmem, hdr, hdr->order);

	kmem_bytes_allocated -= (1 << hdr->order);
}


/**
 * Allocates pages of memory from the kernel memory pool. The number of pages
 * requested must be a power of two and the returned pages will be contiguous
 * in physical memory.
 *
 * Arguments:
 *       [IN] order: Number of pages to allocated, 2^order:
 *                       0 = 1 page
 *                       1 = 2 pages
 *                       2 = 4 pages
 *                       3 = 8 pages
 *                       ...
 * Returns:
 *       Success: Pointer to the start of the allocated memory.
 *       Failure: NULL
 */
void *
kmem_get_pages(unsigned long order)
{
	return buddy_alloc(kmem, order + ilog2(PAGE_SIZE));
}


/**
 * Frees pages of memory previously allocated with kmem_get_pages().
 *
 * Arguments:
 *       [IN] addr:  Address of the memory region to free.
 *       [IN] order: Number of pages to free, 2^order.
 *                   The order must match the value passed to kmem_get_pages()
 *                   when the pages were allocated.
 */
void
kmem_free_pages(void *addr, unsigned long order)
{
	buddy_free(kmem, addr, order + ilog2(PAGE_SIZE));
}


