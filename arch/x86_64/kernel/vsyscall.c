/*
 * Derived from Linux 2.6.25 linux-2.6.25/arch/x86/kernel/vsyscall.c
 * Original header:
 *  Copyright (C) 2001 Andrea Arcangeli <andrea@suse.de> SuSE
 *  Copyright 2003 Andi Kleen, SuSE Labs.
 *
 *  Thanks to hpa@transmeta.com for some useful hint.
 *  Special thanks to Ingo Molnar for his early experience with
 *  a different vsyscall implementation for Linux/IA32 and for the name.
 *
 *  vsyscall 1 is located at -10Mbyte, vsyscall 2 is located
 *  at virtual address -10Mbyte+1024bytes etc... There are at max 4
 *  vsyscalls. One vsyscall can reserve more than 1 slot to avoid
 *  jumping out of line if necessary. We cannot add more with this
 *  mechanism because older kernels won't return -ENOSYS.
 *  If we want more than four we need a vDSO.
 *
 *  Note: the concept clashes with user mode linux. If you use UML and
 *  want per guest time just set the kernel.vsyscall64 sysctl to 0.
 */

#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/time.h>
#include <lwk/unistd.h>
#include <arch/vsyscall.h>
#include <arch/pgtable.h>
#include <arch/fixmap.h>

#define __vsyscall(nr) __attribute__ ((unused,__section__(".vsyscall_" #nr)))
#define __syscall_clobber "r11","cx","memory"

int __vsyscall(0)
vgettimeofday(struct timeval *tv, struct timezone *tz)
{
	int ret;
	asm volatile("syscall"
		: "=a" (ret)
		: "0" (__NR_gettimeofday),"D" (tv),"S" (tz)
		: __syscall_clobber );
	return ret;
}

time_t __vsyscall(1)
vtime(time_t *t)
{
	int ret;
	asm volatile("syscall"
		: "=a" (ret)
		: "0" (__NR_time),"D" (t)
		: __syscall_clobber );
	return ret;
}

void __init
vsyscall_map(void)
{
	extern char __vsyscall_0;
	unsigned long physaddr_page0 = __pa_symbol(&__vsyscall_0);

	/* Setup the virtual syscall fixmap entry */
	__set_fixmap(VSYSCALL_FIRST_PAGE, physaddr_page0, PAGE_KERNEL_VSYSCALL);

	BUG_ON((VSYSCALL_ADDR(0) != __fix_to_virt(VSYSCALL_FIRST_PAGE)));

	BUG_ON((unsigned long) &vgettimeofday !=
			VSYSCALL_ADDR(__NR_vgettimeofday));
	BUG_ON((unsigned long) &vtime !=
			VSYSCALL_ADDR(__NR_vtime));
}

