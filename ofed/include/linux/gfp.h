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

#ifndef	_LINUX_GFP_H_
#define	_LINUX_GFP_H_



#include <linux/page.h>


#define	__GFP_NOWARN	0
#define	__GFP_HIGHMEM	0
#define __GFP_ZERO      ((__force gfp_t)0x8000u) /* Return zeroed page on success */

#define __GFP_HIGH      ((__force gfp_t)0x20u)
#define	GFP_NOWAIT	0
#define	GFP_ATOMIC	(__GFP_HIGH)
#define	GFP_KERNEL	0
#define	GFP_USER	0
#define	GFP_HIGHUSER	0
#define	GFP_HIGHUSER_MOVABLE	0
#define	GFP_IOFS	GFP_ATOMIC
#define GFP_ZERO        __GFP_ZERO

static inline void *
page_address(struct page *page)
{

    return (void *)page->virtual;
}


static inline struct page *
alloc_pages(gfp_t gfp_mask, int order)
{
    struct page * page = kmem_alloc(sizeof(struct page));

    if (!page) {
	goto error;
    }

    /* Pages are always zeroed by Kitten */
    page->virtual = kmem_get_pages(order);
    page->order   = order;
    page->user    = 0;
    
    return page;

 error:
    if (gfp_mask & GFP_ATOMIC)
	    panic("Out of memory! alloc_pages(GFP_ATOMIC) failed.");
    if (page)
	    kmem_free(page);
    return NULL;
}


#define	get_zeroed_pages(mask, order)	alloc_pages(GFP_ZERO,   order);
#define	get_zeroed_page(mask)	alloc_pages(GFP_ZERO,   0);
#define	alloc_page(mask)	alloc_pages(GFP_KERNEL, 0);
#define	__get_free_page(mask)	alloc_pages(GFP_ZERO,   0);

static inline void
free_page(unsigned long addr)
{

	if (addr == 0)
		return;
	kmem_free_pages((void *)addr, 0);
}


static inline void
__free_pages(struct page * page, unsigned int order)
{
	kmem_free_pages(page->virtual, order);
	kmem_free(page);
}


static inline void
free_pages(unsigned long addr, int order) 
{
    kmem_free_pages((void *)addr, order);
}

static inline void
__free_page(struct page * page)
{
	__free_pages(page, 0);
}



#define alloc_pages_node(node, mask, order)     alloc_pages(mask, order)

#define kmalloc_node(chunk, mask, node)         kmalloc(chunk, mask)

#endif	/* _LINUX_GFP_H_ */
