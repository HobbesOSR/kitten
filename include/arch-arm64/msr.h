#ifndef _ARM64_MSR_H
#define _ARM64_MSR_H

#ifndef __ASSEMBLY__

#include <arch-generic/errno-base.h>
/*
 * Access to machine-specific registers (available on 586 and better only)
 * Note: the rd* operations modify the parameters directly (without using
 * pointer indirection), this allows gcc to optimize better
 */




#define __mrs(reg) ({							\
	u64 __val;							\
	asm __volatile__ ("mrs %0, " #reg : "=r" (__val));		\
	__val;								\
})


#define __msr(reg, val)	({						\
	asm __volatile__ ("msr " #reg ", %0" : : "r"(val));		\
})



#define ICC_SRE_EL1     s3_0_c12_c12_5
#define ICC_PMR_EL1     s3_0_c4_c6_0
#define ICC_IAR0_EL1    s3_0_c12_c8_0
#define ICC_EOIR0_EL1   s3_0_c12_c8_1
#define ICC_HPPIR0_EL1  s3_0_c12_c8_2
#define ICC_BPR0_EL1    s3_0_c12_c8_3
#define ICC_RPR_EL1     s3_0_c12_c11_3
#define ICC_IAR1_EL1    s3_0_c12_c12_0
#define ICC_EOIR1_EL1   s3_0_c12_c12_1
#define ICC_HPPIR1_EL1  s3_0_c12_c12_2
#define ICC_BPR1_EL1    s3_0_c12_c12_3
#define ICC_CTLR_EL1    s3_0_c12_c12_4
#define ICC_IGRPEN0_EL1 s3_0_c12_c12_6
#define ICC_IGRPEN1_EL1 s3_0_c12_c12_7

#define mrs(reg)      __mrs(reg)
#define msr(reg, val) __msr(reg, val)


static inline u64
rdtsc( void )
{
	return __mrs(pmccntr_el0);
}


#define rdtscl(val) do {	\
	val = rdtsc(); 		\
} while(0)


#define rdtscll(val) do { 	\
	val = rdtsc();		\
} while(0)

#endif


#endif
