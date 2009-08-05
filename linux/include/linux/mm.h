#ifndef __LINUX_MM_H
#define __LINUX_MM_H

#include <linux/gfp.h>
#include <linux/rbtree.h>

/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr) ALIGN(addr, PAGE_SIZE)

struct page { };

extern void
put_page(struct page *page);

extern void
get_page(struct page *page);

extern int
set_page_dirty_lock(struct page *page);

extern int
can_do_mlock(void);

#endif
