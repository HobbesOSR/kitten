#ifndef _ARM64_MEMORY_H_
#define _ARM64_MEMORY_H_

#include <lwk/sizes.h>
#include <arch/page.h>
#include <arch/pgtable.h>

/*
 * Memory types available.
 */
#define MT_DEVICE_nGnRnE	0
#define MT_DEVICE_nGnRE		1
#define MT_DEVICE_GRE		2
#define MT_NORMAL_NC		3
#define MT_NORMAL		4

#define VA_BITS			(39)

#define _UL(x) _AC(x, UL)


/*
 * Size of the PCI I/O space. This must remain a power of two so that
 * IO_SPACE_LIMIT acts as a mask for the low bits of I/O addresses.
 */
#define PCI_IO_SIZE		SZ_16M


#define MODULES_END     	(PAGE_OFFSET)
#define MODULES_VADDR       (MODULES_END - SZ_64M)
#define EARLYCON_IOBASE     (MODULES_VADDR - SZ_4M)

#define NODES_SHIFT	10 // TODO: Brian was a config variable
#define MAX_NUMNODES    (1 << NODES_SHIFT)


#ifndef __ASSEMBLY__
#include <lwk/types.h>

//#define virt_to_page(kaddr) pfn_to_page(__pa(kaddr) >> PAGE_SHIFT)
#define page_to_phys(page)    ((phys_addr_t)page_to_pfn(page) << PAGE_SHIFT)
#define virt_to_page(addr)      (((unsigned long)(addr)-PAGE_OFFSET) >> PAGE_SHIFT)
#define page_to_virt(page)      __va((unsigned long)(page) << PAGE_SHIFT)

#endif
#endif
