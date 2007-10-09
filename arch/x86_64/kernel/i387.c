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
	extern void __bad_fxsave_alignment(void);
		
	if (offsetof(struct task_struct, arch.thread.i387.fxsave) & 15)
		__bad_fxsave_alignment();
	set_in_cr4(X86_CR4_OSFXSR);     /* enable fast FPU state save/restore */
	set_in_cr4(X86_CR4_OSXMMEXCPT); /* enable unmasked SSE exceptions */

	write_cr0(oldcr0 & ~((1UL<<3)|(1UL<<2))); /* clear TS and EM */

	mxcsr_feature_mask_init();
	/* clean state in init */
	current->arch.status = 0;
	clear_used_math();
}

void init_fpu(struct task_struct *child)
{
	if (tsk_used_math(child)) {
		if (child == current)
			unlazy_fpu(child);
		return;
	}	
	memset(&child->arch.thread.i387.fxsave, 0, sizeof(struct i387_fxsave_struct));
	child->arch.thread.i387.fxsave.cwd = 0x37f;
	child->arch.thread.i387.fxsave.mxcsr = 0x1f80;
	/* only the device not available exception or ptrace can call init_fpu */
	set_stopped_child_used_math(child);
}

/*
 * Signal frame handlers.
 */

int save_i387(struct _fpstate __user *buf)
{
	struct task_struct *tsk = current;
	int err = 0;

	BUILD_BUG_ON(sizeof(struct user_i387_struct) !=
			sizeof(tsk->arch.thread.i387.fxsave));

	if ((unsigned long)buf % 16) 
		printk("save_i387: bad fpstate %p\n",buf); 

	if (!used_math())
		return 0;
	clear_used_math(); /* trigger finit */
	if (tsk->arch.status & TS_USEDFPU) {
		err = save_i387_checking((struct i387_fxsave_struct __user *)buf);
		if (err)
			return err;
		stts();
	} else {
		if (__copy_to_user(buf, &tsk->arch.thread.i387.fxsave, 
				   sizeof(struct i387_fxsave_struct)))
			return -1;
	} 
	return 1;
}

/*
 * ptrace request handlers.
 */

int get_fpregs(struct user_i387_struct __user *buf, struct task_struct *tsk)
{
	init_fpu(tsk);
	return __copy_to_user(buf, &tsk->arch.thread.i387.fxsave,
			       sizeof(struct user_i387_struct)) ? -EFAULT : 0;
}

int set_fpregs(struct task_struct *tsk, struct user_i387_struct __user *buf)
{
	if (__copy_from_user(&tsk->arch.thread.i387.fxsave, buf, 
			     sizeof(struct user_i387_struct)))
		return -EFAULT;
		return 0;
}

/*
 * FPU state for core dumps.
 */

int dump_fpu( struct pt_regs *regs, struct user_i387_struct *fpu )
{
	struct task_struct *tsk = current;

	if (!used_math())
		return 0;

	unlazy_fpu(tsk);
	memcpy(fpu, &tsk->arch.thread.i387.fxsave, sizeof(struct user_i387_struct)); 
	return 1; 
}

int dump_task_fpu(struct task_struct *tsk, struct user_i387_struct *fpu)
{
	int fpvalid = !!tsk_used_math(tsk);

	if (fpvalid) {
		if (tsk == current)
			unlazy_fpu(tsk);
		memcpy(fpu, &tsk->arch.thread.i387.fxsave, sizeof(struct user_i387_struct)); 	
	}
	return fpvalid;
}
