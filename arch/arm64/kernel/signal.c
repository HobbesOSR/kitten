/*
 * Based on arch/arm/kernel/signal.c
 *
 * Copyright (C) 1995-2009 Russell King
 * Copyright (C) 2012 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <lwk/errno.h>
#include <lwk/signal.h>
#include <lwk/aspace.h>

#include <arch/uaccess.h>
#include <arch/elf.h>
#include <arch/cacheflush.h>
#include <arch/unistd.h>
#include <arch/fpsimd.h>
#include <arch-generic/signal.h>


struct ucontext {
	unsigned long	  uc_flags;
	struct ucontext	 *uc_link;
	stack_t		  uc_stack;
	sigset_t	  uc_sigmask;
	/* glibc uses a 1024-bit sigset_t */
	__u8		  __unused[1024 / 8 - sizeof(sigset_t)];
	/* last for future expansion */
	struct sigcontext uc_mcontext;
};

/*
 * Do a signal return; undo the signal stack. These are aligned to 128-bit.
 */

struct rt_sigframe {
	struct siginfo info;
	struct ucontext uc;
	u64 fp;
	u64 lr;
};




static int preserve_fpsimd_context(struct fpsimd_context __user *ctx)
{
	struct fpsimd_state *fpsimd = &current->arch.thread.fpsimd_state;
	int err;

	/* dump the hardware registers to the fpsimd_state structure */
	fpsimd_save_state(fpsimd);

	/* copy the FP and status/control registers */
	err = __copy_to_user(ctx->vregs, fpsimd->vregs, sizeof(fpsimd->vregs));
	__put_user_error(fpsimd->fpsr, &ctx->fpsr, err);
	__put_user_error(fpsimd->fpcr, &ctx->fpcr, err);

	/* copy the magic/size information */
	__put_user_error(FPSIMD_MAGIC, &ctx->head.magic, err);
	__put_user_error(sizeof(struct fpsimd_context), &ctx->head.size, err);

	return err ? -EFAULT : 0;
}

static int restore_fpsimd_context(struct fpsimd_context __user *ctx)
{
	struct fpsimd_state fpsimd;
	__u32 magic, size;
	int err = 0;

	/* check the magic/size information */
	__get_user_error(magic, &ctx->head.magic, err);
	__get_user_error(size, &ctx->head.size, err);
	if (err)
		return -EFAULT;
	if (magic != FPSIMD_MAGIC || size != sizeof(struct fpsimd_context))
		return -EINVAL;

	/* copy the FP and status/control registers */
	err = __copy_from_user(fpsimd.vregs, ctx->vregs,
			       sizeof(fpsimd.vregs));
	__get_user_error(fpsimd.fpsr, &ctx->fpsr, err);
	__get_user_error(fpsimd.fpcr, &ctx->fpcr, err);

	/* load the hardware registers from the fpsimd_state structure */
	if (!err) {
//		preempt_disable();
		fpsimd_load_state(&fpsimd);
//		preempt_enable();
	}

	return err ? -EFAULT : 0;
}

static int restore_sigframe(struct pt_regs *regs,
			    struct rt_sigframe __user *sf)
{
	sigset_t oldset;
	int i, err;
	unsigned long irqstate;
	struct aux_context __user *aux =
		(struct aux_context __user *)sf->uc.uc_mcontext.__reserved;

	err = __copy_from_user(&oldset, &sf->uc.uc_sigmask, sizeof(oldset));
	if (err == 0) {
		spin_lock_irqsave(&current->aspace->lock, irqstate);
       		current->sigblocked = oldset;
		spin_unlock_irqrestore(&current->aspace->lock, irqstate);
	}

	for (i = 0; i < 31; i++)
		__get_user_error(regs->regs[i], &sf->uc.uc_mcontext.regs[i],
				 err);
	__get_user_error(regs->sp, &sf->uc.uc_mcontext.sp, err);
	__get_user_error(regs->pc, &sf->uc.uc_mcontext.pc, err);
	__get_user_error(regs->pstate, &sf->uc.uc_mcontext.pstate, err);

	/*
	 * Avoid sys_rt_sigreturn() restarting.
	 */
	regs->syscallno = ~0UL;

	err |= !valid_user_regs(&regs->user_regs);

	if (err == 0)
		err |= restore_fpsimd_context(&aux->fpsimd);

	return err;
}

asmlinkage long sys_rt_sigreturn(struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;

#if 0
	/* Always make any pending restarted system calls return -EINTR */
	current_thread_info()->restart_block.fn = do_no_restart_syscall;
#endif
	/*
	 * Since we stacked the signal on a 128-bit boundary, then 'sp' should
	 * be word aligned here.
	 */
	if (regs->sp & 15)
		goto badframe;

	frame = (struct rt_sigframe __user *)regs->sp;

	if (!access_ok(VERIFY_READ, frame, sizeof (*frame)))
		goto badframe;

	if (restore_sigframe(regs, frame))
		goto badframe;

#if 0
	if (restore_altstack(&frame->uc.uc_stack))
		goto badframe;
#endif

	return regs->regs[0];

badframe:
#if 0
	if (show_unhandled_signals)
		pr_info_ratelimited("%s[%d]: bad frame in %s: pc=%08llx sp=%08llx\n",
				    current->comm, task_pid_nr(current), __func__,
				    regs->pc, regs->sp);
#endif
	// force_sig(SIGSEGV, current);
	printk("GIVE_SIGSEGV\n");
	return 0;
}

static int setup_sigframe(struct rt_sigframe __user *sf,
			  struct pt_regs *regs, sigset_t *set)
{
	int i, err = 0;
	struct aux_context __user *aux =
		(struct aux_context __user *)sf->uc.uc_mcontext.__reserved;

	/* set up the stack frame for unwinding */
	__put_user_error(regs->regs[29], &sf->fp, err);
	__put_user_error(regs->regs[30], &sf->lr, err);

	for (i = 0; i < 31; i++)
		__put_user_error(regs->regs[i], &sf->uc.uc_mcontext.regs[i],
				 err);
	__put_user_error(regs->sp, &sf->uc.uc_mcontext.sp, err);
	__put_user_error(regs->pc, &sf->uc.uc_mcontext.pc, err);
	__put_user_error(regs->pstate, &sf->uc.uc_mcontext.pstate, err);

	__put_user_error(current->arch.thread.fault_address, &sf->uc.uc_mcontext.fault_address, err);

	err |= __copy_to_user(&sf->uc.uc_sigmask, set, sizeof(*set));

	if (err == 0)
		err |= preserve_fpsimd_context(&aux->fpsimd);

	/* set the "end" magic */
	__put_user_error(0, &aux->end.magic, err);
	__put_user_error(0, &aux->end.size, err);

	return err;
}

static struct rt_sigframe __user *get_sigframe(struct k_sigaction *ka,
					       struct pt_regs *regs)
{
	unsigned long sp, sp_top;
	struct rt_sigframe __user *frame;

	sp = sp_top = regs->sp;

#if 0
	/*
	 * This is the X/Open sanctioned signal stack switching.
	 */
	if ((sigact->sa_flags & SA_ONSTACK) && !sas_ss_flags(sp))
		sp = sp_top = current->sas_ss_sp + current->sas_ss_size;
#endif
	sp = (sp - sizeof(struct rt_sigframe)) & ~15;
	frame = (struct rt_sigframe __user *)sp;

	/*
	 * Check that we can actually write to the signal frame.
	 */
	if (!access_ok(VERIFY_WRITE, frame, sp_top - sp))
		frame = NULL;

	return frame;
}

static void setup_return(struct pt_regs *regs, struct sigaction * sigact,
			 void __user *frame, int usig)
{
	__sigrestore_t sigtramp;

	regs->regs[0] = usig;
	regs->sp = (unsigned long)frame;
	regs->regs[29] = regs->sp + offsetof(struct rt_sigframe, fp);
	regs->pc = (unsigned long)sigact->sa_handler;

#if 0
	if (ka->sa.sa_flags & SA_RESTORER)
		sigtramp = sigact->sa_restorer;
	else
		sigtramp = VDSO_SYMBOL(current->mm->context.vdso, sigtramp);
#else
	sigtramp = sigact->sa_restorer;
#endif


	regs->regs[30] = (unsigned long)sigtramp;
}

static int setup_rt_frame(int usig, struct sigaction * sigact, siginfo_t *info,
			  sigset_t *set, struct pt_regs *regs)
{
	struct rt_sigframe __user *frame;
	int err = 0;

	frame = get_sigframe(sigact, regs);
	if (!frame)
		return 1;

	__put_user_error(0,    &frame->uc.uc_flags, err);
	__put_user_error(NULL, &frame->uc.uc_link,  err);

#if 0
	err |= __save_altstack(&frame->uc.uc_stack, regs->sp);
#endif
	err |= setup_sigframe(frame, regs, set);
	if (err == 0) {
		setup_return(regs, sigact, frame, usig);
		if (sigact->sa_flags & SA_SIGINFO) {
			err |= copy_siginfo_to_user(&frame->info, info);
			regs->regs[1] = (unsigned long)&frame->info;
			regs->regs[2] = (unsigned long)&frame->uc;
		}
	}

	return err;
}

static void setup_restart_syscall(struct pt_regs *regs)
{

	regs->regs[8] = __NR_restart_syscall;
}

/*
 * OK, we're invoking a handler
 */
static void handle_signal(unsigned long sig, struct sigaction *sigact,
			  siginfo_t *info, struct pt_regs *regs)
{
	struct task_struct *tsk = current;
	sigset_t *oldset = &current->sigblocked;
	int usig = sig;
	int ret;

	/*
	 * Set up the stack frame
	 */

	ret = setup_rt_frame(usig, sigact, info, oldset, regs);


	/*
	 * Check that the resulting registers are actually sane.
	 */
	ret |= !valid_user_regs(&regs->user_regs);

	if (ret != 0) {
//		force_sigsegv(sig, tsk);
		printk("Force_sigsegv (Not implemented)\n");
		return;
	}

#if 0
	/*
	 * Fast forward the stepping logic so we step into the signal
	 * handler.
	 */
	user_fastforward_single_step(tsk);
#endif

#if 0
	signal_delivered(sig, info, sigact, regs, 0);
#endif
}

/*
 * Note that 'init' is a special process: it doesn't get signals it doesn't
 * want to handle. Thus you cannot kill init even with a SIGKILL even by
 * mistake.
 *
 * Note that we go through the signals twice: once to check the signals that
 * the kernel can handle, and then we build all the user-level signal handling
 * stack-frames in one go after that.
 */
static void do_signal(struct pt_regs *regs)
{
	unsigned long continue_addr = 0, restart_addr = 0;
	struct sigaction sigact;
	siginfo_t info;
	int signr, retval = 0;
	int syscall = (int)regs->syscallno;

	/*
	 * If we were from a system call, check for system call restarting...
	 */
	if (syscall >= 0) {
		continue_addr = regs->pc;
		restart_addr = continue_addr - (compat_thumb_mode(regs) ? 2 : 4);
		retval = regs->regs[0];

		/*
		 * Avoid additional syscall restarting via ret_to_user.
		 */
		regs->syscallno = ~0UL;

		/*
		 * Prepare for system call restart. We do this here so that a
		 * debugger will see the already changed PC.
		 */
		switch (retval) {
		case -ERESTARTNOHAND:
		case -ERESTARTSYS:
		case -ERESTARTNOINTR:
		case -ERESTART_RESTARTBLOCK:
			regs->regs[0] = regs->orig_x0;
			regs->pc = restart_addr;
			break;
		}
	}

	/*
	 * Get the signal to deliver. When running under ptrace, at this point
	 * the debugger may change all of our registers.
	 */
	signr = sigdequeue(&info, &sigact);
	if (signr > 0) {
		/*
		 * Depending on the signal settings, we may need to revert the
		 * decision to restart the system call, but skip this if a
		 * debugger has chosen to restart at a different PC.
		 */
		if (regs->pc == restart_addr &&
		    (retval == -ERESTARTNOHAND ||
		     retval == -ERESTART_RESTARTBLOCK ||
		     (retval == -ERESTARTSYS &&
		      !(sigact.sa_flags & SA_RESTART)))) {
			regs->regs[0] = -EINTR;
			regs->pc = continue_addr;
		}

		handle_signal(signr, &sigact, &info, regs);
		return;
	}

#if 0
	/*
	 * Handle restarting a different system call. As above, if a debugger
	 * has chosen to restart at a different PC, ignore the restart.
	 */
	if (syscall >= 0 && regs->pc == restart_addr) {
		if (retval == -ERESTART_RESTARTBLOCK)
			setup_restart_syscall(regs);
		user_rewind_single_step(current);
	}
	restore_saved_sigmask();
#endif
	return;
}

asmlinkage void do_notify_resume(struct pt_regs *regs,
				 unsigned int thread_flags)
{
	if (thread_flags & _TIF_SIGPENDING)
		do_signal(regs);

#if 0
	if (thread_flags & _TIF_NOTIFY_RESUME) {
		clear_thread_flag(TIF_NOTIFY_RESUME);
	}
#endif
}