#ifndef _LWK_BOOTMEM_H
#define _LWK_BOOTMEM_H

#include <lwk/list.h>
#include <lwk/init.h>

/**
 * Bootmem control structure.
 *
 * The node_bootmem_map field is a map pointer - the bits represent
 * all physical memory pages (including holes) for the region represented
 * by the enclosing bootmem_data structure.
 */
typedef struct bootmem_data {
	unsigned long	node_boot_start;
	unsigned long	node_low_pfn;
	void		*node_bootmem_map;	// bitmap, one bit per page
	unsigned long	last_offset;
	unsigned long	last_pos;
	unsigned long	last_success;		// previous allocation point,
						// used to speed up search.
	struct list_head list;
} bootmem_data_t;

extern unsigned long __init bootmem_bootmap_pages(unsigned long pages);
extern unsigned long __init init_bootmem(unsigned long start,
                                         unsigned long pages);
extern void __init reserve_bootmem(unsigned long addr, unsigned long size);
extern void * __init alloc_bootmem(unsigned long size);
extern void * __init alloc_bootmem_aligned(unsigned long size,
                                           unsigned long alignment);
extern void __init free_bootmem(unsigned long addr, unsigned long size);
extern unsigned long __init free_all_bootmem(void);

extern void memsys_init(void);

#endif
