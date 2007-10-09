/*
 * Generate definitions needed by assembly language modules.
 * This code generates raw asm output which is post-processed to extract
 * and format the required data.
 */

#if 0
#include <linux/sched.h> 
#include <linux/stddef.h>
#include <linux/errno.h> 
#include <linux/hardirq.h>
#include <linux/suspend.h>
#include <asm/pda.h>
#include <asm/processor.h>
#include <asm/segment.h>
#include <asm/thread_info.h>
#endif

#include <lwk/task.h>
#include <arch/pda.h>

#define DEFINE(sym, val) \
        asm volatile("\n->" #sym " %0 " #val : : "i" (val))

#define BLANK() asm volatile("\n->" : : )

/**
 * This is used to automatically count the number of system calls.
 * A table is generated with one entry for each system call defined in
 * arch/unistd.h, which contains the list of system calls.
 */
#define __NO_STUBS 1
#undef __SYSCALL
#undef _ARCH_X86_64_UNISTD_H
#define __SYSCALL(nr, sym) [nr] = 1,
static char syscalls[] = {
#include <arch/unistd.h>
};

int main(void)
{
#define ENTRY(entry) DEFINE(tsk_ ## entry, offsetof(struct task_struct, entry))
#if 0
	ENTRY(state);
	ENTRY(flags); 
	ENTRY(thread); 
#endif
	ENTRY(pid);
	BLANK();
#undef ENTRY
#define ENTRY(entry) DEFINE(tsk_arch_ ## entry, offsetof(struct task_struct, arch) + offsetof(struct arch_task, entry))
	ENTRY(addr_limit);
	ENTRY(status);
	BLANK();
#undef ENTRY
#define ENTRY(entry) DEFINE(pda_ ## entry, offsetof(struct x8664_pda, entry))
	ENTRY(kernelstack); 
	ENTRY(oldrsp); 
	ENTRY(pcurrent); 
	ENTRY(irqcount);
	ENTRY(cpunumber);
	ENTRY(irqstackptr);
	ENTRY(data_offset);
	BLANK();
#undef ENTRY
#if 0
	DEFINE(pbe_address, offsetof(struct pbe, address));
	DEFINE(pbe_orig_address, offsetof(struct pbe, orig_address));
	DEFINE(pbe_next, offsetof(struct pbe, next));
	BLANK();
	DEFINE(TSS_ist, offsetof(struct tss_struct, ist));
#endif
	BLANK();
	DEFINE(__NR_syscall_max, sizeof(syscalls) - 1);
	return 0;
}
