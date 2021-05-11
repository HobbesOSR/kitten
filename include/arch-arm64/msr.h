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



#define ICC_SRE_EL1 s3_0_c12_c12_5

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
