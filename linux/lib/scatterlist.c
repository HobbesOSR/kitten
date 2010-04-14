/*
 * Copyright (C) 2007 Jens Axboe <jens.axboe@oracle.com>
 *
 * Scatterlist handling helpers.
 *
 * This source code is licensed under the GNU General Public License,
 * Version 2. See the file COPYING for more details.
 */
#include <linux/module.h>
#include <linux/scatterlist.h>

/**
 * sg_init_table - Initialize SG table
 * @sgl:	   The SG table
 * @nents:	   Number of entries in table
 *
 * Notes:
 *   If this is part of a chained sg table, sg_mark_end() should be
 *   used only on the last table part.
 */
void sg_init_table(struct scatterlist *sg, unsigned int nents)
{
	memset(sg, 0, sizeof(*sg) * nents);
}

/**
 * sg_set_page - Set sg entry to point at given page
 * @sg:		 SG entry
 * @page:	 The page
 * @len:	 Length of data
 * @offset:	 Offset into page
 *
 * Description:
 *   Use this function to set an sg entry pointing at a page, never assign
 *   the page directly. We encode sg table information in the lower bits
 *   of the page pointer. See sg_page() for looking up the page belonging
 *   to an sg entry.
 */
void
sg_set_page(struct scatterlist *sg,
            struct page *page, size_t length, size_t offset)
{
	sg->page	=	page;
	sg->address	=	page->virtual;
	sg->length	=	length;
	sg->offset	=	offset;
}

struct page *
sg_page(struct scatterlist *sg)
{
	if (!sg->page)
		panic("sg_page() called on sg with NULL sg->page.");

	return sg->page;
}

/**
 * sg_set_buf - Set sg entry to point at given data
 * @sg:		 SG entry
 * @buf:	 Data
 * @buflen:	 Data length
 */
void
sg_set_buf(struct scatterlist *sg, const void *buf, unsigned int buflen)
{
	sg->page	=	NULL;	
	sg->address	=	(void *)buf;
	sg->length	=	buflen;
	sg->offset	=	offset_in_page(buf);
}

struct scatterlist *sg_next(struct scatterlist *sg)
{
        if (sg_is_last(sg))
                return NULL;

        sg++;
        if (unlikely(sg_is_chain(sg)))
                sg = sg_chain_ptr(sg);

        return sg;
}

struct scatterlist *sg_last(struct scatterlist *sgl, unsigned int nents)
{
        struct scatterlist *sg, *ret = NULL;
        unsigned int i;

        for_each_sg(sgl, sg, nents, i)
                ret = sg;

        return ret;
}

