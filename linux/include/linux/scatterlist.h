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
	dma_addr_t	address;
	size_t		length;
	unsigned int	offset;
};

#define sg_dma_address(sg)	((sg)->address)
#define sg_dma_len(sg)		((sg)->length)

extern struct page *
sg_page(struct scatterlist *sg);

extern void
sg_set_page(struct scatterlist *sg, struct page *page, unsigned int len, unsigned int offset);

extern void
sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen);

extern void
sg_init_table(struct scatterlist *sg, unsigned int nents);

#endif
