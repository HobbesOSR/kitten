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
	struct page *	page;
	void *		address;
	size_t		length;
	size_t		offset;

	dma_addr_t	dma_address;
	size_t		dma_length;
};

#define sg_dma_address(sg)	((sg)->dma_address)
#define sg_dma_len(sg)		((sg)->dma_length)

extern void
sg_init_table(struct scatterlist *sg, unsigned int nents);

extern void
sg_set_page(struct scatterlist *sg, struct page *page, size_t length, size_t offset);

extern struct page *
sg_page(struct scatterlist *sg);

extern void
sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen);

#define for_each_sg(sglist, sg, nr, __i)        \
        for (__i = 0, sg = (sglist); __i < (nr); __i++, sg = sg_next(sg))

#endif
