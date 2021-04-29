/*
 * Based on arch/arm/include/asm/page.h
 *
 * Copyright (C) 1995-2003 Russell King
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ASM_PAGE_H
#define __ASM_PAGE_H
#include <lwk/const.h>

/* PAGE_SHIFT determines the page size */
#ifdef CONFIG_ARM64_64K_PAGES
#define PAGE_SHIFT		16
#else
#define PAGE_SHIFT		12
#endif
#define PAGE_SIZE		(_AC(1,UL) << PAGE_SHIFT)
#define PAGE_MASK		(~(PAGE_SIZE-1))

#define PAGE_SHIFT_4KB 		12
#define PAGE_SHIFT_64KB		16
#define PAGE_SHIFT_2MB		21
#define PAGE_SHIFT_512MB	29
#define PAGE_SHIFT_1GB		30


#define PAGE_SIZE_4KB 		(_AC(1,UL) << PAGE_SHIFT_4KB 	)
#define PAGE_SIZE_64KB		(_AC(1,UL) << PAGE_SHIFT_64KB	)
#define PAGE_SIZE_2MB		(_AC(1,UL) << PAGE_SHIFT_2MB	)
#define PAGE_SIZE_512MB		(_AC(1,UL) << PAGE_SHIFT_512MB	)
#define PAGE_SIZE_1GB		(_AC(1,UL) << PAGE_SHIFT_1GB	)

#define PAGE_MASK_4KB 		(~(PAGE_SIZE_4KB   -1))
#define PAGE_MASK_64KB		(~(PAGE_SIZE_64KB  -1))
#define PAGE_MASK_2MB		(~(PAGE_SIZE_2MB   -1))
#define PAGE_MASK_512MB		(~(PAGE_SIZE_512MB -1))
#define PAGE_MASK_1GB		(~(PAGE_SIZE_1GB   -1))


/* We do define AT_SYSINFO_EHDR but don't use the gate mechanism */
#define __HAVE_ARCH_GATE_AREA		1

/* TODO: Brian adding from original page.h */
#define TASK_ORDER 1
#define TASK_SIZE  (PAGE_SIZE << TASK_ORDER)
#define PAGE_OFFSET		_AC(0xffffffc000000000, UL)
/**
 * The bootloader loads the LWK at address __PHYSICAL_START in physical memory.
 * This must be aligned on a 2 MB page boundary... or else.
 */
#define __PHYSICAL_START	TEXT_OFFSET
#define __START_KERNEL_map	PAGE_OFFSET
#define __START_KERNEL		(__START_KERNEL_map + __PHYSICAL_START)

#ifndef __ASSEMBLY__
/* TODO: Brian adding from original page.h */
/**
 * The kernel is mapped into the virtual address space of every task:
 *
 *     [PAGE_OFFSET, TOP_OF_MEMORY)  Kernel-space virtual memory region
 *     [0, PAGE_OFFSET]              User-space virtual memory region
 */
#include <arch/types.h>
extern phys_addr_t memstart_addr;
#define PHYS_OFFSET             (memstart_addr)
#define __virt_to_phys(x)   (((phys_addr_t)(x) - PAGE_OFFSET + PHYS_OFFSET))
#define __phys_to_virt(x)   ((unsigned long)((x) - PHYS_OFFSET + PAGE_OFFSET))
#define __pa(x)         __virt_to_phys((unsigned long)(x))
#define __va(x)         ((void *)__phys_to_virt((phys_addr_t)(x)))
#define __pa_symbol(x)	__phys_addr((unsigned long)(x))
#ifdef CONFIG_ARM64_64K_PAGES
#include <arch/pgtable-2level-types.h>
#else
#include <arch/pgtable-3level-types.h>
#endif

extern void __cpu_clear_user_page(void *p, unsigned long user);
extern void __cpu_copy_user_page(void *to, const void *from,
				 unsigned long user);
extern void copy_page(void *to, const void *from);
extern void clear_page(void *to);

#define clear_user_page(addr,vaddr,pg)  __cpu_clear_user_page(addr, vaddr)
#define copy_user_page(to,from,vaddr,pg) __cpu_copy_user_page(to, from, vaddr)

typedef struct page * pgtable_t;

#ifdef CONFIG_HAVE_ARCH_PFN_VALID
extern int pfn_valid(unsigned long);
#endif

#include <arch/memory.h>

#endif /* !__ASSEMBLY__ */

#define VM_DATA_DEFAULT_FLAGS \
	(((current->personality & READ_IMPLIES_EXEC) ? VM_EXEC : 0) | \
	 VM_READ | VM_WRITE | VM_MAYREAD | VM_MAYWRITE | VM_MAYEXEC)

/*#include <asm-generic/getorder.h>*/

#endif
