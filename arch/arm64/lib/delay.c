/*
 *	Precise Delay Loops for x86-64
 *
 *	Copyright (C) 1993 Linus Torvalds
 *	Copyright (C) 1997 Martin Mares <mj@atrey.karlin.mff.cuni.cz>
 *
 *	The __delay function must _NOT_ be inlined as its execution time
 *	depends wildly on alignment on many x86 processors. 
 */

#include <lwk/smp.h>
#include <lwk/cpuinfo.h>
#include <lwk/delay.h>
#include <arch/msr.h>
#include <arch/processor.h>
#include <arch/smp.h>

void __delay(unsigned long cycles)
{
	uint64_t bclock, now;

	// rdtscl(bclock);
	bclock = mrs(CNTPCT_EL0);
	do
	{
		rep_nop(); 
		now = mrs(CNTPCT_EL0);
		//rdtscl(now);
	}
	while((now - bclock) < cycles);
}

inline void __const_udelay(unsigned long xsecs)
{
	__delay(
	  (xsecs * cpu_info[this_cpu].arch.tsc_khz * 1000)  /* cycles * 2**32 */
	  >> 32                                             /* div by 2**32 */
	);
}

void __udelay(unsigned long usecs)
{
	__const_udelay(usecs * 0x000010c6);  /* 2**32 / 1000000 */
}

void __ndelay(unsigned long nsecs)
{
	__const_udelay(nsecs * 0x00005);  /* 2**32 / 1000000000 (rounded up) */
}

