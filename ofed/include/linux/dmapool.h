/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _LINUX_DMAPOOL_H_
#define	_LINUX_DMAPOOL_H_

#include <linux/types.h>
#include <linux/io.h>
#include <linux/scatterlist.h>
#include <linux/device.h>
#include <linux/slab.h>

struct dma_pool {
	struct list_head        page_list;
	spinlock_t              lock;
	size_t                  size;
	size_t                  allocation;
	size_t                  boundary;
	struct device *         dev
	char                    name[32];
};


struct dma_page {
	struct list_head        page_list;
	void *                  cpu_addr;
	dma_addr_t              dma_addr;
	unsigned int            in_use;
	unsigned int            offset;
};

static inline struct dma_pool *
dma_pool_create(char *name, struct device *dev, size_t size,
		size_t align, size_t boundary)
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
    pool->size              =       size;
    pool->allocation        =       allocation;
    pool->boundary          =       boundary;
    pool->dev               =       dev;
	
    return pool;
}




static inline void
__pool_initialise_page(struct dma_pool *pool, struct dma_page *page)
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

static inline struct dma_page *
__pool_alloc_page(struct dma_pool *pool, gfp_t mem_flags)
{
	struct dma_page *page;

	page = kmalloc(sizeof(*page), mem_flags);
	if (!page)
		return NULL;
	page->cpu_addr = dma_alloc_coherent(NULL, pool->allocation,
					    &page->dma_addr, mem_flags);
	if (page->cpu_addr) {
#ifdef	DMAPOOL_DEBUG
		memset(page->cpu_addr, POOL_POISON_FREED, pool->allocation);
#endif
		__pool_initialise_page(pool, page);
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
__is_page_busy(struct dma_page *page)
{
	return page->in_use != 0;
}

static inline void
__pool_free_page(struct dma_pool *pool, struct dma_page *page)
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
static inline void
dma_pool_destroy(struct dma_pool *pool)
{
	while (!list_empty(&pool->page_list)) {
		struct dma_page *page;
		page = list_entry(pool->page_list.next,
				  struct dma_page, page_list);
		if (__is_page_busy(page)) {
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
			__pool_free_page(pool, page);
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
static inline void *
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

	page = __pool_alloc_page(pool, GFP_ATOMIC);
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

static inline struct dma_page *
__pool_find_page(struct dma_pool *pool, dma_addr_t dma)
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
static inline void
dma_pool_free(struct dma_pool *pool, void *vaddr, dma_addr_t dma)
{
	struct dma_page *page;
	unsigned long flags;
	unsigned int offset;

	page = __pool_find_page(pool, dma);
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



#endif /* _LINUX_DMAPOOL_H_ */
