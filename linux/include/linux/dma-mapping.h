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

enum dma_data_direction {
	DMA_BIDIRECTIONAL	=	0,
	DMA_TO_DEVICE		=	1,
	DMA_FROM_DEVICE		=	2,
	DMA_NONE		=	3,
};

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
dma_unmap_page(struct device *dev, dma_addr_t dma_address, size_t size, enum dma_data_direction direction);

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

struct dma_attrs { };

#define dma_map_single_attrs(dev, cpu_addr, size, dir, attrs) \
	dma_map_single(dev, cpu_addr, size, dir)

#define dma_unmap_single_attrs(dev, dma_addr, size, dir, attrs) \
	dma_unmap_single(dev, dma_addr, size, dir)

#define dma_map_sg_attrs(dev, sgl, nents, dir, attrs) \
	dma_map_sg(dev, sgl, nents, dir)

#define dma_unmap_sg_attrs(dev, sgl, nents, dir, attrs) \
	dma_unmap_sg(dev, sgl, nents, dir)

#endif
