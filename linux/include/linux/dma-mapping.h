/**
 * \file
 * LWK-specific DMA mapping API header.
 * The API is the same as on Linux but the implementation is different.
 */

#ifndef __LINUX_DMA_MAPPING_H
#define __LINUX_DMA_MAPPING_H

#include <linux/device.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>

/* These definitions mirror those in pci.h, so they can be used
 * interchangeably with their PCI_ counterparts */
enum dma_data_direction {
	DMA_BIDIRECTIONAL	=	0,
	DMA_TO_DEVICE		=	1,
	DMA_FROM_DEVICE		=	2,
	DMA_NONE		=	3,
};

#define DMA_BIT_MASK(n) (((n) == 64) ? ~0ULL : ((1ULL<<(n))-1))

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

extern int
dma_supported(struct device *dev, u64 mask);

extern int
dma_set_mask(struct device *dev, u64 dma_mask);

extern void *
dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag);

extern void
dma_free_coherent(struct device *dev, size_t size, void *cpu_addr, dma_addr_t dma_handle);

extern dma_addr_t
dma_map_single(struct device *dev, void *cpu_addr, size_t size, enum dma_data_direction direction);

extern void
dma_unmap_single(struct device *dev, dma_addr_t dma_addr, size_t size, enum dma_data_direction direction);

extern dma_addr_t
dma_map_page(struct device *dev, struct page *page, unsigned long offset, size_t size, enum dma_data_direction direction); 

extern void
dma_unmap_page(struct device *dev, dma_addr_t dma_addr, size_t size, enum dma_data_direction direction);

extern int
dma_map_sg(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction direction);

extern void
dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nhwentries, enum dma_data_direction direction);

extern void
dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma_handle, size_t size, enum dma_data_direction direction);

extern void
dma_sync_single_for_device(struct device *dev, dma_addr_t dma_handle, size_t size, enum dma_data_direction direction);

extern void
dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg, int nelems, enum dma_data_direction direction);

extern void
dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg, int nelems, enum dma_data_direction direction);

extern int
dma_mapping_error(struct device *dev, dma_addr_t dma_addr);

extern int
dma_get_cache_alignment(void);

struct dma_attrs;

#define dma_map_single_attrs(dev, cpu_addr, size, dir, attrs) \
	dma_map_single(dev, cpu_addr, size, dir)

#define dma_unmap_single_attrs(dev, dma_addr, size, dir, attrs) \
	dma_unmap_single(dev, dma_addr, size, dir)

#define dma_map_sg_attrs(dev, sgl, nents, dir, attrs) \
	dma_map_sg(dev, sgl, nents, dir)

#define dma_unmap_sg_attrs(dev, sgl, nents, dir, attrs) \
	dma_unmap_sg(dev, sgl, nents, dir)

/* Backwards compat, remove in 2.7.x */
#define dma_sync_single         dma_sync_single_for_cpu
#define dma_sync_sg             dma_sync_sg_for_cpu

extern u64 dma_get_required_mask(struct device *dev);

static inline unsigned int dma_get_max_seg_size(struct device *dev)
{
	return dev->dma_parms ? dev->dma_parms->max_segment_size : 65536;
}

static inline unsigned int dma_set_max_seg_size(struct device *dev,
						unsigned int size)
{
	if (dev->dma_parms) {
		dev->dma_parms->max_segment_size = size;
		return 0;
	} else
		return -EIO;
}

static inline unsigned long dma_get_seg_boundary(struct device *dev)
{
	return dev->dma_parms ?
		dev->dma_parms->segment_boundary_mask : 0xffffffff;
}

static inline int dma_set_seg_boundary(struct device *dev, unsigned long mask)
{
	if (dev->dma_parms) {
		dev->dma_parms->segment_boundary_mask = mask;
		return 0;
	} else
		return -EIO;
}

#endif
