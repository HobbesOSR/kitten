#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <asm/io.h>

int
dma_supported(struct device *dev, u64 mask)
{
	if (mask != DMA_64BIT_MASK) {
		printk(KERN_WARNING "dma_supported() called with non 64-bit mask.\n");
		return 0;
	}

	return 1;
}

int
dma_set_mask(struct device *dev, u64 dma_mask)
{
	if (!dev->dma_mask || !dma_supported(dev, dma_mask))
		return -EIO;

	*dev->dma_mask = dma_mask;

	return 0;
}

void *
dma_alloc_coherent(struct device *dev, size_t size, dma_addr_t *dma_handle, gfp_t flag)
{
	void *addr = (void *)get_zeroed_pages(flag, get_order(size));
	if (!addr)
		return NULL;

	*dma_handle = virt_to_bus(addr);

	return addr;
}

void
dma_free_coherent(struct device *dev, size_t size, void *cpu_addr, dma_addr_t dma_handle)
{
	free_pages((unsigned long)cpu_addr, get_order(size));
}

dma_addr_t
dma_map_single(struct device *dev, void *cpu_addr, size_t size, enum dma_data_direction direction)
{
	return virt_to_bus(cpu_addr);
}

void
dma_unmap_single(struct device *dev, dma_addr_t dma_addr, size_t size, enum dma_data_direction direction)
{
	/* Nothing to do */
}

dma_addr_t
dma_map_page(struct device *dev, struct page *page, unsigned long offset, size_t size, enum dma_data_direction direction)
{
	return dma_map_single(dev, page->virtual + offset, size, direction);
}

void
dma_unmap_page(struct device *dev, dma_addr_t dma_addr, size_t size, enum dma_data_direction direction)
{
	dma_unmap_single(dev, dma_addr, size, direction);
}

int
dma_map_sg(struct device *dev, struct scatterlist *sg, int nents, enum dma_data_direction direction)
{
	int i;

	for (i = 0; i < nents; i++) {
		struct scatterlist *s = &sg[i];

		s->dma_address = virt_to_bus(s->address) + s->offset;
		s->dma_length  = s->length;
	}

	return nents;
}

void
dma_unmap_sg(struct device *dev, struct scatterlist *sg, int nhwentries, enum dma_data_direction direction)
{
	/* Nothing to do */
}

void
dma_sync_single_for_cpu(struct device *dev, dma_addr_t dma_handle, size_t size, enum dma_data_direction direction)
{
	/* Nothing to do */
}

void
dma_sync_single_for_device(struct device *dev, dma_addr_t dma_handle, size_t size, enum dma_data_direction direction)
{
	/* Nothing to do */
}

void
dma_sync_sg_for_cpu(struct device *dev, struct scatterlist *sg, int nelems, enum dma_data_direction direction)
{
	/* Nothing to do */
}

void
dma_sync_sg_for_device(struct device *dev, struct scatterlist *sg, int nelems, enum dma_data_direction direction)
{
	/* Nothing to do */
}

int
dma_mapping_error(struct device *dev, dma_addr_t dma_addr)
{
	return (dma_addr == 0);
}

int
dma_get_cache_alignment(void)
{
	return boot_cpu_data.arch.x86_clflush_size;
}
