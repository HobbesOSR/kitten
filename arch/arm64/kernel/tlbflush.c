#include <lwk/tlbflush.h>
#include <lwk/cpuinfo.h>
#include <lwk/xcall.h>
#include <arch/processor.h>


/**
 * Flush all non-global entries in the calling CPU's TLB.
 *
 * Flushing non-global entries is the common-case since user-space
 * does not use global pages (i.e., pages mapped at the same virtual
 * address in *all* processes).
 *
 * Use __flush_tlb_kernel() to flush all TLB entries, including global entries.
 */
void
__flush_tlb(void)
{
	uint64_t tmpreg;
#if 0
	__asm__ __volatile__(
		"movq %%cr3, %0;  # flush TLB \n"
		"movq %0, %%cr3;              \n"
		: "=r" (tmpreg)
		:: "memory"
	);
#endif
}


/**
 * flush_tlb() cross-call handler.
 */
static void
do_flush_tlb_xcall(void *info)
{
	__flush_tlb();
}


/**
 * Flush all non-global entries from all TLBs in the system.
 */
void
flush_tlb(void)
{
	xcall_function(cpu_online_map, do_flush_tlb_xcall, NULL, 1);
}


/**
 * Flush all entries in the calling CPU's TLB, including global entries.
 *
 * This function must be used if the kernel page tables are updated,
 * since the kernel identity map pages are marked global.
 *
 * Use __flush_tlb() if only non-global entries need to be flushed.
 */
void
__flush_tlb_kernel(void)
{
	uint64_t tmpreg, cr4, cr4_orig;

	/*
	 * Global pages have to be flushed a bit differently. Not a real
	 * performance problem because this does not happen often.
	 */
#if 0
	__asm__ __volatile__(
		"movq %%cr4, %2;  # turn off PGE     \n"
		"movq %2, %1;                        \n"
		"andq %3, %1;                        \n"
		"movq %1, %%cr4;                     \n"
		"movq %%cr3, %0;  # flush TLB        \n"
		"movq %0, %%cr3;                     \n"
		"movq %2, %%cr4;  # turn PGE back on \n"
		: "=&r" (tmpreg), "=&r" (cr4), "=&r" (cr4_orig)
		: "i" (~X86_CR4_PGE)
		: "memory"
	);
#endif
}


/**
 * flush_tlb_kernel() cross-call handler.
 */
static void
do_flush_tlb_kernel_xcall(void *info)
{
	__flush_tlb_kernel();
}


/**
 * Flush all entries from all TLBs in the system, including global entries.
 */
void
flush_tlb_kernel(void)
{
	xcall_function(cpu_online_map, do_flush_tlb_kernel_xcall, NULL, 1);
}
