/**
 * \file
 * LWK-specific scatterlist API header.
 * The API is the same as on Linux but the implementation is different.
 */

#ifndef __LINUX_SCATTERLIST_H
#define __LINUX_SCATTERLIST_H

#include <linux/types.h>
#include <linux/string.h>

struct scatterlist {
	paddr_t		address;
	size_t		length;
};

#define sg_dma_address(sg)	((sg)->address)
#define sg_dma_len(sg)		((sg)->length)

struct page *
sg_page(struct scatterlist *sg);

#endif
