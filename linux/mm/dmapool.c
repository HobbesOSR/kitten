/* LWK specific implementation of the dma_pool API */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/dmapool.h>

struct dma_pool {
	struct list_head	page_list;
	spinlock_t		lock;
	size_t			size;
	size_t			allocation;
	size_t			boundary;
	struct device *		dev;
	char			name[32];
};

struct dma_page {
	struct list_head	page_list;
	void *			cpu_addr;
	dma_addr_t		dma_addr;
	unsigned int		in_use;
	unsigned int		offset;
};

/**
 * dma_pool_create - Creates a pool of consistent memory blocks, for dma.
 * @name: name of pool, for diagnostics
 * @dev: device that will be doing the DMA
 * @size: size of the blocks in this pool.
 * @align: alignment requirement for blocks; must be a power of two
 * @boundary: returned blocks won't cross this power of two boundary
 * Context: !in_interrupt()
 *
 * Returns a dma allocation pool with the requested characteristics, or
 * null if one can't be created.  Given one of these pools, dma_pool_alloc()
 * may be used to allocate memory.  Such memory will all have "consistent"
 * DMA mappings, accessible by the device and its driver without using
 * cache flushing primitives.  The actual size of blocks allocated may be
 * larger than requested because of alignment.
 *
 * If @boundary is nonzero, objects returned from dma_pool_alloc() won't
 * cross that size boundary.  This is useful for devices which have
 * addressing restrictions on individual DMA transfers, such as not crossing
 * boundaries of 4KBytes.
 */
struct dma_pool *
dma_pool_create(const char *name, struct device *dev,
		size_t size, size_t align, size_t boundary)
{
	struct dma_pool *pool;
	size_t allocation;

	if (align == 0) {
		align = 1;
	} else if (align & (align - 1)) {
		return NULL;
	}

	if (size == 0) {
		return NULL;
	} else if (size < 4) {
		size = 4;
	}

	if ((size % align) != 0)
		size = ALIGN(size, align);

	allocation = max_t(size_t, size, PAGE_SIZE);

	if (!boundary) {
		boundary = allocation;
	} else if ((boundary < size) || (boundary & (boundary - 1))) {
		return NULL;
	}

	pool = kmalloc(sizeof(*pool), GFP_KERNEL);
	if (!pool)
		return pool;

	strlcpy(pool->name, name, sizeof(pool->name));

	INIT_LIST_HEAD(&pool->page_list);
	spin_lock_init(&pool->lock);
	pool->size		=	size;
	pool->allocation	=	allocation;
	pool->boundary		=	boundary;
	pool->dev		=	dev;

	return pool;
}

static void
pool_initialise_page(struct dma_pool *pool, struct dma_page *page)
{
	unsigned int offset = 0;
	unsigned int next_boundary = pool->boundary;

	do {
		unsigned int next = offset + pool->size;
		if (unlikely((next + pool->size) >= next_boundary)) {
			next = next_boundary;
			next_boundary += pool->boundary;
		}
		*(int *)(page->cpu_addr + offset) = next;
		offset = next;
	} while (offset < pool->allocation);
}

static struct dma_page *
pool_alloc_page(struct dma_pool *pool, gfp_t mem_flags)
{
	struct dma_page *page;

	page = kmalloc(sizeof(*page), mem_flags);
	if (!page)
		return NULL;
	page->cpu_addr = dma_alloc_coherent(pool->dev, pool->allocation,
					    &page->dma_addr, mem_flags);
	if (page->cpu_addr) {
#ifdef	DMAPOOL_DEBUG
		memset(page->cpu_addr, POOL_POISON_FREED, pool->allocation);
#endif
		pool_initialise_page(pool, page);
		list_add(&page->page_list, &pool->page_list);
		page->in_use = 0;
		page->offset = 0;
	} else {
		kfree(page);
		page = NULL;
	}
	return page;
}

static inline int
is_page_busy(struct dma_page *page)
{
	return page->in_use != 0;
}

static void
pool_free_page(struct dma_pool *pool, struct dma_page *page)
{
#ifdef	DMAPOOL_DEBUG
	memset(page->cpu_addr, POOL_POISON_FREED, pool->allocation);
#endif
	dma_free_coherent(pool->dev, pool->allocation, page->cpu_addr, page->dma_addr);
	list_del(&page->page_list);
	kfree(page);
}

/**
 * dma_pool_destroy - destroys a pool of dma memory blocks.
 * @pool: dma pool that will be destroyed
 * Context: !in_interrupt()
 *
 * Caller guarantees that no more memory from the pool is in use,
 * and that nothing will try to use the pool after this call.
 */
void
dma_pool_destroy(struct dma_pool *pool)
{
	while (!list_empty(&pool->page_list)) {
		struct dma_page *page;
		page = list_entry(pool->page_list.next,
				  struct dma_page, page_list);
		if (is_page_busy(page)) {
			if (pool->dev)
				dev_err(pool->dev,
					"dma_pool_destroy %s, %p busy\n",
					pool->name, page->cpu_addr);
			else
				printk(KERN_ERR
				       "dma_pool_destroy %s, %p busy\n",
				       pool->name, page->cpu_addr);
			/* leak the still-in-use consistent memory */
			list_del(&page->page_list);
			kfree(page);
		} else
			pool_free_page(pool, page);
	}

	kfree(pool);
}

/**
 * dma_pool_alloc - get a block of consistent memory
 * @pool: dma pool that will produce the block
 * @mem_flags: GFP_* bitmask
 * @handle: pointer to dma address of block
 *
 * This returns the kernel virtual address of a currently unused block,
 * and reports its dma address through the handle.
 * If such a memory block can't be allocated, %NULL is returned.
 */
void *
dma_pool_alloc(struct dma_pool *pool, gfp_t mem_flags, dma_addr_t *handle)
{
	unsigned long flags;
	struct dma_page *page;
	size_t offset;
	void *retval;

	spin_lock_irqsave(&pool->lock, flags);

	list_for_each_entry(page, &pool->page_list, page_list) {
		if (page->offset < pool->allocation)
			goto ready;
	}

	page = pool_alloc_page(pool, GFP_ATOMIC);
	if (!page) {
		retval = NULL;
		goto done;
	}

 ready:
	page->in_use++;
	offset = page->offset;
	page->offset = *(int *)(page->cpu_addr + offset);
	retval = offset + page->cpu_addr;
	*handle = offset + page->dma_addr;
#ifdef	DMAPOOL_DEBUG
	memset(retval, POOL_POISON_ALLOCATED, pool->size);
#endif
 done:
	spin_unlock_irqrestore(&pool->lock, flags);
	return retval;
}

static struct dma_page *
pool_find_page(struct dma_pool *pool, dma_addr_t dma)
{
	unsigned long flags;
	struct dma_page *page;

	spin_lock_irqsave(&pool->lock, flags);
	list_for_each_entry(page, &pool->page_list, page_list) {
		if (dma < page->dma_addr)
			continue;
		if (dma < (page->dma_addr + pool->allocation))
			goto done;
	}
	page = NULL;
 done:
	spin_unlock_irqrestore(&pool->lock, flags);
	return page;
}

/**
 * dma_pool_free - put block back into dma pool
 * @pool: the dma pool holding the block
 * @vaddr: virtual address of block
 * @dma: dma address of block
 *
 * Caller promises neither device nor driver will again touch this block
 * unless it is first re-allocated.
 */
void
dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t dma)
{
	struct dma_page *page;
	unsigned long flags;
	unsigned int offset;

	page = pool_find_page(pool, dma);
	if (!page) {
		if (pool->dev)
			dev_err(pool->dev,
				"dma_pool_free %s, %p/%lx (bad dma)\n",
				pool->name, vaddr, (unsigned long)dma);
		else
			printk(KERN_ERR "dma_pool_free %s, %p/%lx (bad dma)\n",
			       pool->name, vaddr, (unsigned long)dma);
		return;
	}

	offset = vaddr - page->cpu_addr;
#ifdef	DMAPOOL_DEBUG
	if ((dma - page->dma_addr) != offset) {
		if (pool->dev)
			dev_err(pool->dev,
				"dma_pool_free %s, %p (bad vaddr)/%Lx\n",
				pool->name, vaddr, (unsigned long long)dma);
		else
			printk(KERN_ERR
			       "dma_pool_free %s, %p (bad vaddr)/%Lx\n",
			       pool->name, vaddr, (unsigned long long)dma);
		return;
	}
	{
		unsigned int chain = page->offset;
		while (chain < pool->allocation) {
			if (chain != offset) {
				chain = *(int *)(page->cpu_addr + chain);
				continue;
			}
			if (pool->dev)
				dev_err(pool->dev, "dma_pool_free %s, dma %Lx "
					"already free\n", pool->name,
					(unsigned long long)dma);
			else
				printk(KERN_ERR "dma_pool_free %s, dma %Lx "
					"already free\n", pool->name,
					(unsigned long long)dma);
			return;
		}
	}
	memset(vaddr, POOL_POISON_FREED, pool->size);
#endif

	spin_lock_irqsave(&pool->lock, flags);
	page->in_use--;
	*(int *)vaddr = page->offset;
	page->offset = offset;

	/*
	 * Resist a temptation to do
	 *    if (!is_page_busy(page)) pool_free_page(pool, page);
	 * Better have a few empty pages hang around.
	 */
	spin_unlock_irqrestore(&pool->lock, flags);
}
