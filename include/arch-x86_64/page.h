#ifndef _X86_64_PAGE_H
#define _X86_64_PAGE_H

#define PAGE_SHIFT		12
#define PAGE_SIZE		__PAGE_SIZE
#define PAGE_OFFSET		__PAGE_OFFSET


/**
 * CAREFUL... Each of the following defines has two versions, one for
 *            assembly and another for C.  Be sure to update both!
 */
#ifndef __ASSEMBLY__
#define __PAGE_SIZE             (1UL << PAGE_SHIFT)
#define __PHYSICAL_START        ((unsigned long)CONFIG_PHYSICAL_START)
#define __START_KERNEL          (__START_KERNEL_map + __PHYSICAL_START)
#define __START_KERNEL_map      0xffffffff80000000UL
#define __PAGE_OFFSET           0xffff810000000000UL
#else
#define __PAGE_SIZE             (0x1 << PAGE_SHIFT)
#define __PHYSICAL_START        CONFIG_PHYSICAL_START
#define __START_KERNEL          (__START_KERNEL_map + __PHYSICAL_START)
#define __START_KERNEL_map      0xffffffff80000000
#define __PAGE_OFFSET           0xffff810000000000
#endif

#define PAGE_MASK		(~(PAGE_SIZE-1))
#define PHYSICAL_PAGE_MASK	(~(PAGE_SIZE-1) & __PHYSICAL_MASK)

/* See Documentation/x86_64/mm.txt for a description of the memory map. */
#define __PHYSICAL_MASK_SHIFT   46
#define __PHYSICAL_MASK     ((1UL << __PHYSICAL_MASK_SHIFT) - 1)
#define __VIRTUAL_MASK_SHIFT    48
#define __VIRTUAL_MASK      ((1UL << __VIRTUAL_MASK_SHIFT) - 1)

#define TASK_ORDER 1 
#define TASK_SIZE  (PAGE_SIZE << TASK_ORDER)
#define CURRENT_MASK (~(TASK_SIZE-1))

#define EXCEPTION_STACK_ORDER 0
#define EXCEPTION_STKSZ (PAGE_SIZE << EXCEPTION_STACK_ORDER)

#define DEBUG_STACK_ORDER EXCEPTION_STACK_ORDER
#define DEBUG_STKSZ (PAGE_SIZE << DEBUG_STACK_ORDER)

#define IRQSTACK_ORDER 2
#define IRQSTACKSIZE (PAGE_SIZE << IRQSTACK_ORDER)

/* Note: __pa(&symbol_visible_to_c) should be always replaced with __pa_symbol.
   Otherwise you risk miscompilation. */
#define __pa(x)		(((unsigned long)(x)>=__START_KERNEL_map)?(unsigned long)(x) - (unsigned long)__START_KERNEL_map:(unsigned long)(x) - PAGE_OFFSET)
/* __pa_symbol should be used for C visible symbols.
   This seems to be the official gcc blessed way to do such arithmetic. */
#define __pa_symbol(x)				\
	({unsigned long v;			\
	  asm("" : "=r" (v) : "0" (x));		\
	  __pa(v); })


#define __va(x)                 ((void *)((unsigned long)(x)+PAGE_OFFSET))
#define __boot_va(x)            __va(x)
#define __boot_pa(x)            __pa(x)

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

#endif

#define PTRS_PER_PGD	512
#define KERNEL_TEXT_SIZE (40UL*1024*1024)

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


#endif
