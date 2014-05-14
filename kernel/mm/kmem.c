/* Copyright (c) 2007, Sandia National Laboratories */

#include <lwk/kernel.h>
#include <lwk/buddy.h>
#include <lwk/log2.h>
#include <lwk/spinlock.h>


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
 * Lock for serializing access to kmem and kmem_bytes_allocated.
 */
static DEFINE_SPINLOCK(kmem_lock);


/**
 * Total number of bytes in the kernel memory pool.
 */
static unsigned long kmem_bytes_managed;


/**
 * Total number of bytes allocated from the kernel memory pool.
 */
static unsigned long kmem_bytes_allocated = 0;


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
 * region that is at least 16-byte aligned. The memory returned is zeroed.
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
	unsigned long flags;

	/* Make room for block header */
	size += sizeof(struct kmem_block_hdr);

	/* Calculate the block order needed */
	order = ilog2(roundup_pow_of_two(size));
	if (order < MIN_ORDER)
		order = MIN_ORDER;

	/* Allocate memory from the underlying buddy system */
	spin_lock_irqsave(&kmem_lock, flags);
	hdr = buddy_alloc(kmem, order);
	if (hdr)
		kmem_bytes_allocated += (1UL << order);
	spin_unlock_irqrestore(&kmem_lock, flags);
	if (hdr == NULL)
		return NULL;

	/* Zero the block */
	memset(hdr, 0, (1UL << order));

	/* Initialize the block header */
	hdr->order = order;       /* kmem_free() needs this to free the block */
	hdr->magic = KMEM_MAGIC;  /* used for sanity check */

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
kmem_free(
	const void *		addr
)
{
	struct kmem_block_hdr *hdr;
	unsigned long flags;

	if( !addr )
		return;

	BUG_ON((unsigned long)addr < sizeof(struct kmem_block_hdr));

	/* Find the block header */
	hdr = (struct kmem_block_hdr *)addr - 1;
	BUG_ON(hdr->magic != KMEM_MAGIC);

	/* Return block to the underlying buddy system */
	spin_lock_irqsave(&kmem_lock, flags);
	kmem_bytes_allocated -= (1UL << hdr->order);
	buddy_free(kmem, hdr, hdr->order);
	spin_unlock_irqrestore(&kmem_lock, flags);
}


/**
 * Allocates pages of memory from the kernel memory pool. The number of pages
 * requested must be a power of two and the returned pages will be contiguous
 * in physical memory. The memory returned is zeroed.
 *
 * \returns Pointer to the start of the allocated memory on succcess
 * or NULL for failure..
 */
void *
kmem_get_pages(
	/** Number of pages to allocated, 2^order:
	 * - 0 = 1 page
	 * - 1 = 2 pages
	 * - 2 = 4 pages
	 * - 3 = 8 pages
	 * - ...
	 */
	unsigned long order
)
{
	unsigned long block_order;
	void *addr;
	unsigned long flags;

	/* Calculate the block size needed; convert page order to byte order */
	block_order = order + ilog2(PAGE_SIZE);

	/* Allocate memory from the underlying buddy system */
	spin_lock_irqsave(&kmem_lock, flags);
	addr = buddy_alloc(kmem, block_order);
	if (addr)
		kmem_bytes_allocated += (1UL << block_order);
	spin_unlock_irqrestore(&kmem_lock, flags);
	if (addr == NULL)
		return NULL;

	/* Zero the block and return its address */
	memset(addr, 0, (1UL << block_order));
	return addr;
}


/**
 * Frees pages of memory previously allocated with kmem_get_pages().
 */
void
kmem_free_pages(
	/**  Address of the memory region to free. */
	const void *		addr,

	/** Number of pages to free, 2^order.
	 * The order must match the value passed to kmem_get_pages()
	 * when the pages were allocated.
	 */
	unsigned long		order
)
{
	unsigned long flags;

	spin_lock_irqsave(&kmem_lock, flags);
	kmem_bytes_allocated -= (1UL << order);
	buddy_free(kmem, addr, order + ilog2(PAGE_SIZE));
	spin_unlock_irqrestore(&kmem_lock, flags);
}


/**
 * Returns true if the physical addr passed in is kmem, false otherwise.
 */
bool
paddr_is_kmem(
	const paddr_t		paddr
) 
{
	return (((unsigned long)__va(paddr) >= kmem->base_addr) && 
	       ((unsigned long)__va(paddr) - kmem->base_addr) < (1 << (kmem->pool_order)));
}


 
