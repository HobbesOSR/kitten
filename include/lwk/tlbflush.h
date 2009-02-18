#ifndef _LWK_TLBFLUSH_H
#define _LWK_TLBFLUSH_H

/**
 * TLB flush API. Affects all TLBs in the system.
 * @{
 */
extern void flush_tlb(void);
extern void flush_tlb_kernel(void);
// @}

/**
 * Local TLB flush API. Affects only the calling CPU's TLB.
 * @{
 */
extern void __flush_tlb(void);
extern void __flush_tlb_kernel(void);
// @}

#endif /* _LWK_TLBFLUSH_H */
