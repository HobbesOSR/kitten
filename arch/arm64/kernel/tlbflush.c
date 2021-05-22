#include <lwk/tlbflush.h>
#include <lwk/cpuinfo.h>
#include <lwk/xcall.h>
#include <arch/processor.h>
#include <arch/barrier.h>
#include <arch/tlbflush.h>







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
	flush_tlb_all();
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
