/*
 * Based on arch/arm/include/asm/tlbflush.h
 *
 * Copyright (C) 1999-2003 Russell King
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
#ifndef __ASM_TLBFLUSH_H
#define __ASM_TLBFLUSH_H

#ifndef __ASSEMBLY__

#include <lwk/sched.h>
#include <lwk/aspace.h>

#include <arch/cputype.h>
#include <arch/mmu.h>
#include <arch/barrier.h>
#include <arch/alternative.h>


extern void __cpu_flush_user_tlb_range(unsigned long, unsigned long, struct vm_area_struct *);
extern void __cpu_flush_kern_tlb_range(unsigned long, unsigned long);

extern struct cpu_tlb_fns cpu_tlb;

/*
 * Raw TLBI operations.
 *
 * Where necessary, use the __tlbi() macro to avoid asm()
 * boilerplate. Drivers and most kernel code should use the TLB
 * management routines in preference to the macro below.
 *
 * The macro can be used as __tlbi(op) or __tlbi(op, arg), depending
 * on whether a particular TLBI operation takes an argument or
 * not. The macros handles invoking the asm with or without the
 * register argument as appropriate.
 */
#define __TLBI_0(op, arg) asm ("tlbi " #op "\n"				       \
		   ALTERNATIVE("nop\n			nop",		       \
			       "dsb ish\n		tlbi " #op,	       \
			       ARM64_WORKAROUND_REPEAT_TLBI,		       \
			       CONFIG_QCOM_FALKOR_ERRATUM_1009)		       \
			    : : )

#define __TLBI_1(op, arg) asm ("tlbi " #op ", %0\n"			       \
		   ALTERNATIVE("nop\n			nop",		       \
			       "dsb ish\n		tlbi " #op ", %0",     \
			       ARM64_WORKAROUND_REPEAT_TLBI,		       \
			       CONFIG_QCOM_FALKOR_ERRATUM_1009)		       \
			    : : "r" (arg))

#define __TLBI_N(op, arg, n, ...) __TLBI_##n(op, arg)

#define __tlbi(op, ...)		__TLBI_N(op, ##__VA_ARGS__, 1, 0)




/*
 *	TLB Management
 *	==============
 *
 *	The arch/arm64/mm/tlb.S files implement these methods.
 *
 *	The TLB specific code is expected to perform whatever tests it needs
 *	to determine if it should invalidate the TLB for each call.  Start
 *	addresses are inclusive and end addresses are exclusive; it is safe to
 *	round these addresses down.
 *
 *	flush_tlb_all()
 *
 *		Invalidate the entire TLB.
 *
 *	flush_tlb_mm(mm)
 *
 *		Invalidate all TLB entries in a particular address space.
 *		- mm	- mm_struct describing address space
 *
 *	flush_tlb_range(mm,start,end)
 *
 *		Invalidate a range of TLB entries in the specified address
 *		space.
 *		- mm	- mm_struct describing address space
 *		- start - start address (may not be aligned)
 *		- end	- end address (exclusive, may not be aligned)
 *
 *	flush_tlb_page(vaddr,vma)
 *
 *		Invalidate the specified page in the specified address range.
 *		- vaddr - virtual address (may not be aligned)
 *		- vma	- vma_struct describing address range
 *
 *	flush_kern_tlb_page(kaddr)
 *
 *		Invalidate the TLB entry for the specified page.  The address
 *		will be in the kernels virtual memory space.  Current uses
 *		only require the D-TLB to be invalidated.
 *		- kaddr - Kernel virtual memory address
 */

static inline void local_flush_tlb_all(void)
{
	dsb(nshst);
	__tlbi(vmalle1);
	dsb(nsh);
	isb();
}

static inline void flush_tlb_all(void)
{
	dsb(ishst);
	__tlbi(vmalle1is);
	dsb(ish);
	isb();
}


static inline void flush_tlb_mm(struct aspace *mm)
{
	unsigned long asid = (unsigned long)ASID(mm) << 48;

	dsb(ishst);
	__tlbi(aside1is, asid);
	dsb(ish);
}

static inline void flush_tlb_page(struct vm_area_struct *vma,
				  unsigned long uaddr)
{
	unsigned long addr; /* = uaddr >> 12 |
			((unsigned long)ASID(vma->vm_mm) << 48);*/

/*** *** ***/
	panic("flush_tlb_page() not implemented....\n");
	asm volatile("b . \n");
/*** *** ***/


	dsb(ishst);
	__tlbi(vae1is, addr);
	dsb(ish);
}



/**
 * Flush all entries in the calling CPU's TLB, including global entries.
 *
 * This function must be used if the kernel page tables are updated,
 * since the kernel identity map pages are marked global.
 *
 * Use __flush_tlb() if only non-global entries need to be flushed.
 */
static inline void
__flush_tlb_kernel(void)
{
	dsb(ishst);
//	__tlbi(vaae1is, addr);
	__tlbi(alle1is); // or vmalle1is ??
	dsb(ish);
	isb();
}


/*
 * Convert calls to our calling convention.
 */
#define flush_tlb_range(vma,start,end)	__cpu_flush_user_tlb_range(start,end,vma)
#define flush_tlb_kernel_range(s,e)	__cpu_flush_kern_tlb_range(s,e)

/*
 * On AArch64, the cache coherency is handled via the set_pte_at() function.
 */
static inline void update_mmu_cache(struct vm_area_struct *vma,
				    unsigned long addr, pte_t *ptep)
{
	/*
	 * set_pte() does not have a DSB, so make sure that the page table
	 * write is visible.
	 */
	dsb(ishst);
}

#endif

#endif
