/* LWK specific implementation of the gfp API */

#include <lwk/kernel.h>
#include <linux/gfp.h>

struct page *
alloc_pages(gfp_t gfp_mask, unsigned int order)
{
	struct page *page = kmem_alloc(sizeof(struct page));
	if (!page)
		goto error;

	page->virtual = kmem_get_pages(order);
	if (!page->virtual)
		goto error;

	page->order = order;
	page->user  = 0;

	return page;

error:
	if (gfp_mask & GFP_ATOMIC)
		panic("Out of memory! alloc_pages(GFP_ATOMIC) failed.");
	if (page)
		kmem_free(page);
	return NULL;
}

struct page *
alloc_page(gfp_t gfp_mask)
{
	return alloc_pages(gfp_mask, 0);
}

void
__free_pages(struct page *page, unsigned int order)
{
	kmem_free_pages(page->virtual, order);
	kmem_free(page);
}

void
__free_page(struct page *page)
{
	__free_pages(page, 0);
}

unsigned long
get_zeroed_pages(gfp_t gfp_mask, unsigned int order)
{
	void *addr = kmem_get_pages(order);
	if (!addr && (gfp_mask & GFP_ATOMIC))
		panic("Out of memory! get_zeroed_pages(GFP_ATOMIC) failed.");
	return (unsigned long)addr;
}

unsigned long
get_zeroed_page(gfp_t gfp_mask)
{
	return get_zeroed_pages(gfp_mask, 0);
}

void
free_pages(unsigned long addr, unsigned int order)
{
	kmem_free_pages((void *)addr, 0);
}

void
free_page(unsigned long addr)
{
	free_pages(addr, 0);
}

unsigned long
__get_free_pages(gfp_t gfp_mask, unsigned int order)
{
	return get_zeroed_pages(gfp_mask, order);
}

unsigned long
__get_free_page(gfp_t gfp_mask)
{
	return get_zeroed_pages(gfp_mask, 0);
}
