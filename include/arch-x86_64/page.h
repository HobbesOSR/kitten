#ifndef _X86_64_PAGE_H
#define _X86_64_PAGE_H

#include <lwk/const.h>

/**
 * Define the base page size, 4096K on x86_64.
 * PAGE_SHIFT defines the base page size.
 */
#define PAGE_SHIFT		12
#define PAGE_SIZE		(_AC(1,UL) << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))
#define PHYSICAL_PAGE_MASK	(~(PAGE_SIZE-1) & __PHYSICAL_MASK)

/**
 * The kernel is mapped into the virtual address space of every task:
 *
 *     [PAGE_OFFSET, TOP_OF_MEMORY)  Kernel-space virtual memory region
 *     [0, PAGE_OFFSET]              User-space virtual memory region
 */
#define PAGE_OFFSET		_AC(0xffff810000000000, UL)

/**
 * The bootloader loads the LWK at address __PHYSICAL_START in physical memory.
 * This must be aligned on a 2 MB page boundary... or else.
 */
#define __PHYSICAL_START	CONFIG_PHYSICAL_START
#define __KERNEL_ALIGN		0x200000

/**
 * If you hit this error when compiling the LWK, change your config file so that
 * CONFIG_PHYSICAL_START is aligned to a 2 MB boundary.
 */
#if (CONFIG_PHYSICAL_START % __KERNEL_ALIGN) != 0
#error "CONFIG_PHYSICAL_START must be a multiple of 2MB"
#endif

/**
 * The kernel page tables map the kernel image text and data starting at
 * virtual address __START_KERNEL_map. The kernel text starts at
 * __START_KERNEL.
 */
#define __START_KERNEL_map	_AC(0xffffffff80000000, UL)
#define __START_KERNEL		(__START_KERNEL_map + __PHYSICAL_START)

/* See Documentation/x86_64/mm.txt for a description of the memory map. */
#define __PHYSICAL_MASK_SHIFT   46
#define __PHYSICAL_MASK     ((_AC(1,UL) << __PHYSICAL_MASK_SHIFT) - 1)
#define __VIRTUAL_MASK_SHIFT    48
#define __VIRTUAL_MASK      ((_AC(1,UL) << __VIRTUAL_MASK_SHIFT) - 1)

#define TASK_ORDER 1 
#define TASK_SIZE  (PAGE_SIZE << TASK_ORDER)
#define CURRENT_MASK (~(TASK_SIZE-1))

#define EXCEPTION_STACK_ORDER 0
#define EXCEPTION_STKSZ (PAGE_SIZE << EXCEPTION_STACK_ORDER)

#define DEBUG_STACK_ORDER EXCEPTION_STACK_ORDER
#define DEBUG_STKSZ (PAGE_SIZE << DEBUG_STACK_ORDER)

#define IRQSTACK_ORDER 2
#define IRQSTACKSIZE (PAGE_SIZE << IRQSTACK_ORDER)

#define STACKFAULT_STACK	1
#define DOUBLEFAULT_STACK	2
#define NMI_STACK		3
#define DEBUG_STACK		4
#define MCE_STACK		5
#define N_EXCEPTION_STACKS	5	/* hw limit is 7 */

/**
 * Macros for converting between physical address and kernel virtual address.
 * NOTE: These only work for kernel virtual addresses in the identity map.
 */
#ifndef __ASSEMBLY__
extern unsigned long __phys_addr(unsigned long virt_addr);
#endif
#define __pa(x)		__phys_addr((unsigned long)(x))
#define __pa_symbol(x)	__phys_addr((unsigned long)(x))
#define __va(x)		((void *)((unsigned long)(x)+PAGE_OFFSET))
#define __boot_va(x)	__va(x)
#define __boot_pa(x)	__pa(x)

#ifndef __ASSEMBLY__
/*
 * These are used to make use of C type-checking..
 */
typedef struct { unsigned long pte; } pte_t;
typedef struct { unsigned long pmd; } pmd_t;
typedef struct { unsigned long pud; } pud_t;
typedef struct { unsigned long pgd; } pgd_t;
#define PTE_MASK    PHYSICAL_PAGE_MASK


extern pud_t level3_kernel_pgt[512];
extern pud_t level3_physmem_pgt[512];
extern pud_t level3_ident_pgt[512];
extern pmd_t level2_kernel_pgt[512];
extern pgd_t init_level4_pgt[];
extern pgd_t boot_level4_pgt[];

typedef struct { unsigned long pgprot; } pgprot_t;

extern unsigned long end_pfn;

#endif

#define PTRS_PER_PGD	512
#define KERNEL_TEXT_SIZE (40*1024*1024)

#define pte_val(x)  ((x).pte)
#define pmd_val(x)  ((x).pmd)
#define pud_val(x)  ((x).pud)
#define pgd_val(x)  ((x).pgd)
#define pgprot_val(x)   ((x).pgprot)

#define __pte(x) ((pte_t) { (x) } )
#define __pmd(x) ((pmd_t) { (x) } )
#define __pud(x) ((pud_t) { (x) } )
#define __pgd(x) ((pgd_t) { (x) } )
#define __pgprot(x) ((pgprot_t) { (x) } )

#define LARGE_PAGE_MASK (~(LARGE_PAGE_SIZE-1))
#define LARGE_PAGE_SIZE (_AC(1,UL) << PMD_SHIFT)

#endif
