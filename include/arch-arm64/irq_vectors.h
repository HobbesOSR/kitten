#ifndef _ARM64_IRQ_VECTORS_H
#define _ARM64_IRQ_VECTORS_H




/*
 * [64,238]  Free for use by devices
 * [239,255] Used by LWK for various internal purposes
 */
#if 0
#define INVALIDATE_TLB_0_VECTOR			240
#define INVALIDATE_TLB_1_VECTOR			241
#define INVALIDATE_TLB_2_VECTOR			242
#define INVALIDATE_TLB_3_VECTOR			243
#define INVALIDATE_TLB_4_VECTOR			244
#define INVALIDATE_TLB_5_VECTOR			245
#define INVALIDATE_TLB_6_VECTOR			246
#define INVALIDATE_TLB_7_VECTOR			247
#endif

/**
 * Meta-defines describing the interrupt vector space defined above.
 */
#define NUM_IRQ_ENTRIES				1024
#define SGI_VECTOR_START      			0
#define SGI_VECTOR_COUNT			16
#define PPI_VECTOR_START			16
#define PPI_VECTOR_COUNT			16
#define SPI_VECTOR_START			32
#define SPI_VECTOR_COUNT			992
#define LPI_VECTOR_START			8192

#define FIRST_AVAIL_VECTOR			(SGI_VECTOR_START)
#define FIRST_SYSTEM_VECTOR			(SGI_VECTOR_START + SGI_VECTOR_COUNT)		


#define NUM_IPI_ENTRIES				16
#define LWK_XCALL_FUNCTION_VECTOR		1
#define LWK_XCALL_RESCHEDULE_VECTOR		2


#if 0
#define FIRST_SYSTEM_VECTOR			239
#define INVALIDATE_TLB_VECTOR_START		240
#define INVALIDATE_TLB_VECTOR_END		247
#define NUM_INVALIDATE_TLB_VECTORS		8
#endif

#endif
