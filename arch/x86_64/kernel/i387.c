/*
 *  linux/arch/x86_64/kernel/i387.c
 *
 *  Copyright (C) 1994 Linus Torvalds
 *  Copyright (C) 2002 Andi Kleen, SuSE Labs
 *
 *  Pentium III FXSR, SSE support
 *  General FPU state handling cleanups
 *	Gareth Hughes <gareth@valinux.com>, May 2000
 * 
 *  x86-64 rework 2002 Andi Kleen. 
 *  Does direct fxsave in and out of user space now for signal handlers.
 *  All the FSAVE<->FXSAVE conversion code has been moved to the 32bit emulation,
 *  the 64bit user space sees a FXSAVE frame directly. 
 */

#include <lwk/task.h>
#include <lwk/init.h>
#include <arch/processor.h>
#include <arch/i387.h>
#include <arch/sigcontext.h>
#include <arch/user.h>
#include <arch/ptrace.h>
#include <arch/uaccess.h>

unsigned int mxcsr_feature_mask __read_mostly = 0xffffffff;

void mxcsr_feature_mask_init(void)
{
	unsigned int mask;
	clts();
	memset(&current->arch.thread.i387.fxsave, 0, sizeof(struct i387_fxsave_struct));
	asm volatile("fxsave %0" : : "m" (current->arch.thread.i387.fxsave));
	mask = current->arch.thread.i387.fxsave.mxcsr_mask;
	if (mask == 0) mask = 0x0000ffbf;
	mxcsr_feature_mask &= mask;
	stts();
}

/*
 * Called at bootup to set up the initial FPU state that is later cloned
 * into all processes.
 */
void __cpuinit fpu_init(void)
{
	unsigned long oldcr0 = read_cr0();
	unsigned int avx_support;
	extern void __bad_fxsave_alignment(void);
		
	if (offsetof(struct task_struct, arch.thread.i387.fxsave) & 15)
		__bad_fxsave_alignment();
	set_in_cr4(X86_CR4_OSFXSR);     /* enable fast FPU state save/restore */
	set_in_cr4(X86_CR4_OSXMMEXCPT); /* enable unmasked SSE exceptions */

	write_cr0(oldcr0 & ~((1UL<<3)|(1UL<<2))); /* clear TS and EM */
	avx_support = cpuid_ecx(0x80000001);
	if((avx_support & 0x18001000UL) == 0x18001000UL) {
		set_in_cr4(X86_CR4_OSXSAVE);
		xsetbv(XCR_XFEATURE_ENABLED_MASK, XSTATE_YMM|XSTATE_SSE|XSTATE_FP);
	}
	mxcsr_feature_mask_init();

	clts();
}


void
reinit_fpu_state(struct task_struct *tsk)
{
	memset(&tsk->arch.thread.i387.fxsave, 0, sizeof(struct i387_fxsave_struct));
	tsk->arch.thread.i387.fxsave.cwd = 0x37f;
	tsk->arch.thread.i387.fxsave.mxcsr = 0x1f80;
}


// Used by signal delivery path to save FPU state on the user stack
int save_i387(struct _fpstate __user *buf)
{
	int err;

	if ((unsigned long)buf % 16)
		printk("save_i387: bad fpstate %p\n",buf);

	// Stash the caller's FPU state on the user-level stack
	err = save_i387_checking((struct i387_fxsave_struct __user *)buf);
	if (err)
		return err;

	// Reinitialize FPU state, so signal handler can use floating-point ops
	reinit_fpu_state(current);
	fpu_restore_state(current);

	return 0;
}


// Used by sys_sigreturn to restore FPU state from the user stack
int restore_i387(struct _fpstate __user *buf)
{
	int err;

	err = restore_fpu_checking((__force struct i387_fxsave_struct *)buf);
	if (err)
		return err;

	return 0;
}
