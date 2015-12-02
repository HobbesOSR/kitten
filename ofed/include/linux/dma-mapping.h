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
#ifndef	_LINUX_DMA_MAPPING_H_
#define _LINUX_DMA_MAPPING_H_

#include <linux/types.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/dma-attrs.h>
#include <linux/scatterlist.h>
#include <linux/mm.h>
#include <linux/page.h>

/*
#include <sys/system.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/pmap.h>
*/

enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3,
};

struct dma_map_ops {
    void * (*alloc_coherent) (struct device * dev, 
			      size_t          size,
			      dma_addr_t    * dma_handle,
			      gfp_t           gfp);
    
    void (*free_coherent) (struct device * dev, 
			   size_t          size,
			   void          * vaddr, 
			   dma_addr_t      dma_handle);

    dma_addr_t (*map_page) (struct device           * dev,
			    struct page             * page,
			    unsigned long             offset, 
			    size_t                    size,
			    enum dma_data_direction   dir,
			    struct dma_attrs        * attrs);

    void (*unmap_page)(struct device *dev, dma_addr_t dma_handle,
		       size_t size, enum dma_data_direction dir, struct dma_attrs *attrs);
    int (*map_sg)(struct device *dev, struct scatterlist *sg,
		  int nents, enum dma_data_direction dir, struct dma_attrs *attrs);
    void (*unmap_sg)(struct device *dev, struct scatterlist *sg, int nents,
		     enum dma_data_direction dir, struct dma_attrs *attrs);
    void (*sync_single_for_cpu)(struct device *dev, dma_addr_t dma_handle,
				size_t size, enum dma_data_direction dir);
    void (*sync_single_for_device)(struct device *dev,
				   dma_addr_t dma_handle, size_t size, enum dma_data_direction dir);
    void (*sync_single_range_for_cpu)(struct device *dev,
				      dma_addr_t dma_handle, unsigned long offset, size_t size,
				      enum dma_data_direction dir);
    void (*sync_single_range_for_device)(struct device *dev,
					 dma_addr_t dma_handle, unsigned long offset, size_t size,
					 enum dma_data_direction dir);
    void (*sync_sg_for_cpu)(struct device *dev, struct scatterlist *sg,
			    int nents, enum dma_data_direction dir);
    void (*sync_sg_for_device)(struct device *dev, struct scatterlist *sg,
			       int nents, enum dma_data_direction dir);
    int (*mapping_error)(struct device *dev, dma_addr_t dma_addr);
    int (*dma_supported)(struct device *dev, u64 mask);

    int is_phys;
};


#define	DMA_BIT_MASK(n)	(((n) == 64) ? ~0ULL : ((1ULL << (n)) - 1))


/*
 * NOTE: do not use the below macros in new code and do not add new definitions
 * here.
 *
 * Instead, just open-code DMA_BIT_MASK(n) within your driver
 */
#define DMA_64BIT_MASK  DMA_BIT_MASK(64)
#define DMA_48BIT_MASK  DMA_BIT_MASK(48)
#define DMA_47BIT_MASK  DMA_BIT_MASK(47)
#define DMA_40BIT_MASK  DMA_BIT_MASK(40)
#define DMA_39BIT_MASK  DMA_BIT_MASK(39)
#define DMA_35BIT_MASK  DMA_BIT_MASK(35)
#define DMA_32BIT_MASK  DMA_BIT_MASK(32)
#define DMA_31BIT_MASK  DMA_BIT_MASK(31)
#define DMA_30BIT_MASK  DMA_BIT_MASK(30)
#define DMA_29BIT_MASK  DMA_BIT_MASK(29)
#define DMA_28BIT_MASK  DMA_BIT_MASK(28)
#define DMA_24BIT_MASK  DMA_BIT_MASK(24)

#define DMA_MASK_NONE   0x0ULL



#define dma_map_single(d, a, s, r) dma_map_single_attrs(d, a, s, r, NULL)
#define dma_unmap_single(d, a, s, r) dma_unmap_single_attrs(d, a, s, r, NULL)
#define dma_map_sg(d, s, n, r) dma_map_sg_attrs(d, s, n, r, NULL)
#define dma_unmap_sg(d, s, n, r) dma_unmap_sg_attrs(d, s, n, r, NULL)

#define	DEFINE_DMA_UNMAP_ADDR(name)		dma_addr_t name
#define	DEFINE_DMA_UNMAP_LEN(name)		__u32 name
#define	dma_unmap_addr(p, name)			((p)->name)
#define	dma_unmap_addr_set(p, name, v)		(((p)->name) = (v))
#define	dma_unmap_len(p, name)			((p)->name)
#define	dma_unmap_len_set(p, name, v)		(((p)->name) = (v))



static inline int
dma_supported(struct device *dev, u64 mask)
{
	if (mask != DMA_64BIT_MASK) {
		printk(KERN_WARNING "dma_supported() called with non 64-bit mask.\n");
		return 0;
	}

	/* XXX busdma takes care of this elsewhere. */
	return (1);
}
 
static inline int
dma_set_mask(struct device *dev, u64 dma_mask)
{

	if (!dev->dma_mask || !dma_supported(dev, dma_mask))
		return -EIO;

	*dev->dma_mask = dma_mask;
	return (0);
}

static inline int
dma_set_coherent_mask(struct device *dev, u64 mask)
{

	if (!dma_supported(dev, mask))
		return -EIO;
	/* XXX Currently we don't support a seperate coherent mask. */
	return 0;
}

static inline void *
dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle,
		   gfp_t flag)
{
	/*
	vm_paddr_t high;
	size_t align;
	void *mem;

	if (dev->dma_mask)
		high = *dev->dma_mask;
	else
		high = BUS_SPACE_MAXADDR_32BIT;
	align = PAGE_SIZE << get_order(size);
	mem = (void *)kmem_alloc_contig(kmem_arena, size, flag, 0, high, align,
	    0, VM_MEMATTR_DEFAULT);
	if (mem)
		*dma_handle = vtophys(mem);
	else
		*dma_handle = 0;
	return (mem);
	*/

	void *addr = (void *)get_zeroed_pages(flag, get_order(size));
	if (!addr)
		return NULL;
	
	*dma_handle = virt_to_bus(addr);
	
	return addr;
}
                       
static inline void
dma_free_coherent(struct device *dev, size_t size, void *cpu_addr,
		  dma_addr_t dma_handle)
{
	/*
	kmem_free(kmem_arena, (vm_offset_t)cpu_addr, size);
	*/

	free_pages((unsigned long)cpu_addr, get_order(size));

}

/* XXX This only works with no iommu. */
static inline dma_addr_t
dma_map_single_attrs(struct device *dev, void *ptr, size_t size,
		     enum dma_data_direction dir, struct dma_attrs *attrs)
{

	return virt_to_bus(ptr);
}

static inline void
dma_unmap_single_attrs(struct device *dev, dma_addr_t addr, size_t size,
		       enum dma_data_direction dir, struct dma_attrs *attrs)
{
}

static inline int
dma_map_sg_attrs(struct device *dev, struct scatterlist *sgl, int nents,
    enum dma_data_direction dir, struct dma_attrs *attrs)
{
	struct scatterlist *sg;
	int i;
	
	for_each_sg(sgl, sg, nents, i)
		sg_dma_address(sg) = sg_phys(sg);

	return (nents);
}

static inline void
dma_unmap_sg_attrs(struct device *dev, struct scatterlist *sg, int nents,
		   enum dma_data_direction dir, struct dma_attrs *attrs)
{
}
 
static inline dma_addr_t
dma_map_page(struct device *dev, struct page *page,
	     unsigned long offset, size_t size, enum dma_data_direction direction)
{

	return dma_map_single(dev, page->virtual + offset, size, direction);
}

static inline void
dma_unmap_page(struct device *dev, dma_addr_t dma_address, size_t size,
	       enum dma_data_direction direction)
{
	dma_unmap_single(dev, dma_address, size, direction);
}

static inline void
dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma_handle, size_t size,
			enum dma_data_direction direction)
{
}

static inline void
dma_sync_single(struct device *dev, dma_addr_t addr, size_t size,
		enum dma_data_direction dir)
{
	dma_sync_single_for_cpu(dev, addr, size, dir);
}

static inline void
dma_sync_single_for_device(struct device *dev, dma_addr_t dma_handle,
			   size_t size, enum dma_data_direction direction)
{
}

static inline void
dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg, int nelems,
		    enum dma_data_direction direction)
{
}

static inline void
dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg, int nelems,
		       enum dma_data_direction direction)
{
}

static inline void
dma_sync_single_range_for_cpu(struct device *dev, dma_addr_t dma_handle,
			      unsigned long offset, size_t size, int direction)
{
}

static inline void
dma_sync_single_range_for_device(struct device *dev, dma_addr_t dma_handle,
				 unsigned long offset, size_t size, int direction)
{
}

static inline int
dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{

	return (0);
}

static inline unsigned int dma_set_max_seg_size(struct device *dev,
                                                 unsigned int size)
{
        return (0);
}


static inline int
dma_get_cache_alignment(void)
{
	return boot_cpu_data.arch.x86_clflush_size;
}


#endif	/* _LINUX_DMA_MAPPING_H_ */
