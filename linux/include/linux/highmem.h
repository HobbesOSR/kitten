/* Empty file */
#include <linux/memory.h>
static inline void *kmap(struct page *page)
{
	might_sleep();
	return page_address(page);
}

static inline void kunmap(struct page *page)
{
}

