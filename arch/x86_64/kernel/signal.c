#include <lwk/kernel.h>
#include <lwk/signal.h>
#include <lwk/aspace.h>
#include <lwk/ptrace.h>
#include <arch/i387.h>


typedef struct signalstack {
	void __user *		ss_sp;
	int			ss_flags;
	size_t			ss_size;
} stack_t;


struct ucontext {
	unsigned long		uc_flags;
	struct ucontext *	uc_link;
	stack_t			uc_stack;
	struct sigcontext	uc_mcontext;
	sigset_t		uc_sigmask;   // last for extensibility
};


struct rt_sigframe {
	char __user *		pretcode;
	struct ucontext		uc;
	struct siginfo		siginfo;
};


static void __user *
get_stack(
	struct pt_regs *	regs,
	size_t			size
)
{
	unsigned long rsp;

	// Use the normal user stack,
	// minus the redzone buffer required by the x86_64 ABI
	rsp = regs->rsp - 128;

	return (void __user *)round_down(rsp - size, 16);
}


static int
save_ptregs_to_user_stack(
	struct sigcontext __user *	sc,
	struct pt_regs *		regs
)
{
	int err = 0;

	err |= __put_user( regs->r15    , &sc->r15    );
	err |= __put_user( regs->r14    , &sc->r14    );
	err |= __put_user( regs->r13    , &sc->r13    );
	err |= __put_user( regs->r12    , &sc->r12    );
	err |= __put_user( regs->rbp    , &sc->rbp    );
	err |= __put_user( regs->rbx    , &sc->rbx    );
	err |= __put_user( regs->r11    , &sc->r11    );
	err |= __put_user( regs->r10    , &sc->r10    );
	err |= __put_user( regs->r9     , &sc->r9     );
	err |= __put_user( regs->r8     , &sc->r8     );
	err |= __put_user( regs->rax    , &sc->rax    );
	err |= __put_user( regs->rcx    , &sc->rcx    );
	err |= __put_user( regs->rdx    , &sc->rdx    );
	err |= __put_user( regs->rsi    , &sc->rsi    );
	err |= __put_user( regs->rdi    , &sc->rdi    );
	err |= __put_user( regs->rip    , &sc->rip    );
	err |= __put_user( regs->eflags , &sc->eflags );
	err |= __put_user( regs->rsp    , &sc->rsp    );

	return err;
}


static int
restore_ptregs_from_user_stack(
	struct sigcontext __user *	sc,
	struct pt_regs *		regs
)
{
	int err = 0;
	unsigned int existing_eflags = regs->eflags;

	err |= __get_user( regs->r15    , &sc->r15    );
	err |= __get_user( regs->r14    , &sc->r14    );
	err |= __get_user( regs->r13    , &sc->r13    );
	err |= __get_user( regs->r12    , &sc->r12    );
	err |= __get_user( regs->rbp    , &sc->rbp    );
	err |= __get_user( regs->rbx    , &sc->rbx    );
	err |= __get_user( regs->r11    , &sc->r11    );
	err |= __get_user( regs->r10    , &sc->r10    );
	err |= __get_user( regs->r9     , &sc->r9     );
	err |= __get_user( regs->r8     , &sc->r8     );
	err |= __get_user( regs->rax    , &sc->rax    );
	err |= __get_user( regs->rcx    , &sc->rcx    );
	err |= __get_user( regs->rdx    , &sc->rdx    );
	err |= __get_user( regs->rsi    , &sc->rsi    );
	err |= __get_user( regs->rdi    , &sc->rdi    );
	regs->orig_rax = -1;   // disable syscall checks
	err |= __get_user( regs->rip    , &sc->rip    );
	regs->cs = __USER_CS;  // hard-code to use user-space code segment
	err |= __get_user( regs->eflags , &sc->eflags );
	err |= __get_user( regs->rsp    , &sc->rsp    );
	regs->ss = __USER_DS;  // hard-code to use user-space data segment

	// build eflags based on eflags on entry, and eflags stashed in user-space
	regs->eflags = (existing_eflags & ~0x40DD5) | (regs->eflags & 0x40DD5);

	return err;
}


static int
save_fpu_state_to_user_stack(
	struct sigcontext __user *	sc,
	struct _fpstate __user *	fpstate
)
{
	if (__put_user(fpstate, &sc->fpstate))
		return -EFAULT;

	return save_i387(fpstate);
}


static int
restore_fpu_state_from_user_stack(
	struct sigcontext __user *	sc
)
{
	struct _fpstate __user *fpstate;

	if (__get_user(fpstate, &sc->fpstate))
		return -EFAULT;

	return restore_i387(fpstate);
}


static int
save_misc_state_to_user_stack(
	struct rt_sigframe __user *	frame,
	sigset_t *			oldset
)
{
	int err = 0;
	struct sigcontext __user *sc = &frame->uc.uc_mcontext;

	err |= __put_user(0, &frame->uc.uc_flags);
	err |= __put_user(0, &frame->uc.uc_link);
	err |= __put_user(0, &frame->uc.uc_stack.ss_sp);
	err |= __put_user(1, &frame->uc.uc_stack.ss_flags);
	err |= __put_user(0, &frame->uc.uc_stack.ss_size);
	err |= __put_user(__USER_CS, &sc->cs);
	err |= __put_user(0, &sc->gs);
	err |= __put_user(0, &sc->fs);
	err |= __put_user(current->arch.thread.trap_no, &sc->trapno);
	err |= __put_user(current->arch.thread.error_code, &sc->err);
	err |= __put_user(current->arch.thread.cr2, &sc->cr2);
	err |= __put_user(oldset->bitmap[0], &sc->oldmask);

	return err;
}


static int
setup_signal_frame(
	unsigned long		signum,
	siginfo_t *		siginfo,
	struct sigaction *	sigact,
	sigset_t *		oldset,
	struct pt_regs *	regs
)
{
	struct rt_sigframe __user *frame;
	struct _fpstate __user *fp;
	int err = 0;

	fp = get_stack(regs, sizeof(struct _fpstate));
	frame = (void __user *)round_down(
		(unsigned long)fp - sizeof(struct rt_sigframe), 16) - 8;

	if (!access_ok(VERIFY_WRITE, fp, sizeof(struct _fpstate)))
		goto give_sigsegv;

	if (!access_ok(VERIFY_WRITE, frame, sizeof(*frame)))
		goto give_sigsegv;

	// Save essential state needed by sys_rt_sigreturn() to user-stack
	err |= save_ptregs_to_user_stack(&frame->uc.uc_mcontext, regs);
	err |= save_fpu_state_to_user_stack(&frame->uc.uc_mcontext, fp);
	err |= __copy_to_user(&frame->uc.uc_sigmask, oldset, sizeof(*oldset));

	// Save other misc-state to user-stack. Userspace might expect
	// this stuff but kernel does not need it for sys_rt_sigreturn()
	err |= save_misc_state_to_user_stack(frame, oldset);
	err |= copy_siginfo_to_user(&frame->siginfo, siginfo);

	// Setup for return from userspace signal handler.
	// When signal handler returns, &frame->pretcode is executed,
	// which then calls sys_rt_sigreturn().
	err |= __put_user(sigact->sa_restorer, &frame->pretcode);

	if (err)
		goto give_sigsegv;

	// Set up registers for signal handler
	regs->rsp = (unsigned long)frame;
	regs->cs  = __USER_CS;
	regs->rax = 0;
	regs->rdi = signum;				// ARG0
	regs->rsi = (unsigned long)&frame->siginfo;	// ARG1
	regs->rdx = (unsigned long)&frame->uc;		// ARG2
	regs->rip = (unsigned long)sigact->sa_handler;	// signal handler()

	// This has nothing to do with segment registers -
	// see include/asm-x86_64/uaccess.h for details.
	set_fs(USER_DS);

	return 1;

give_sigsegv:
	//force_sigsegv(signum, current)
	printk("GIVE_SIGSEGV\n");
	return 0;
}


static int
handle_signal(
	unsigned long		signum,
	siginfo_t *		siginfo,
	struct sigaction *	sigact,
	sigset_t *		oldset,
	struct pt_regs *	regs
)
{
	int status;
	unsigned long irqstate;

	// Did we come here from a system call?
	if ((long)regs->orig_rax >= 0) {
		// If so, and if requested, setup to restart the interrupted
		// system call automatically once the signal handler is finished
		switch ((long)(int)regs->rax) {
		case -ERESTARTNOHAND:
			sigprocmask(SIG_SETMASK, &current->saved_sigmask, NULL);
			regs->rax = (unsigned)-EINTR;
			break;
			case -ERESTARTSYS:
				if (!(sigact->sa_flags & SA_RESTART)) {
					regs->rax = (unsigned)-EINTR;
					break;
				}
				// fallthrough
			case -ERESTARTNOINTR:
				regs->rax = regs->orig_rax;
				regs->rip -= 2;
				break;
		}
	}

	status = setup_signal_frame(signum, siginfo, sigact, oldset, regs);

	if (status) {
		spin_lock_irqsave(&current->aspace->lock, irqstate);
		sigset_or(&current->sigblocked, &current->sigblocked, &sigact->sa_mask);
		if (!(sigact->sa_flags & SA_NODEFER))
			sigset_add(&current->sigblocked, signum);
		spin_unlock_irqrestore(&current->aspace->lock, irqstate);
	}

	return status;
}


int
do_signal(
	struct pt_regs *	regs
)
{
	struct sigaction sigact;
	siginfo_t siginfo;
	int signum;
	sigset_t *oldset = &current->sigblocked;

	signum = sigdequeue(&siginfo, &sigact);
	if (signum > 0)
		return handle_signal(signum, &siginfo, &sigact, oldset, regs);

	// No signal needed to be handled.
	// If we got here from an interrupted system call, and if
	// requested, setup to automatically restart the original system
	// call that was interrupted by the (bogus/not really there) signal.
	if ((long)regs->orig_rax >= 0) {
		long res = regs->rax;
		if (res == -ERESTARTSYS) {
			regs->rax = regs->orig_rax;
			regs->rip -= 2;
		}
	}

	return 0;
}


long
sys_rt_sigreturn(
	struct pt_regs *	regs
)
{
	struct rt_sigframe __user *frame;
	sigset_t oldset;
	unsigned long irqstate;

	frame = (struct rt_sigframe __user *)(regs->rsp - 8);
	if (!access_ok(VERIFY_READ, frame, sizeof(*frame)))
		goto badframe;

	if (__copy_from_user(&oldset, &frame->uc.uc_sigmask, sizeof(oldset)))
		goto badframe;

	sigset_del(&oldset, SIGKILL);
	sigset_del(&oldset, SIGSTOP);

	spin_lock_irqsave(&current->aspace->lock, irqstate);
        current->sigblocked = oldset;
	spin_unlock_irqrestore(&current->aspace->lock, irqstate);

        if (restore_ptregs_from_user_stack(&frame->uc.uc_mcontext, regs))
                goto badframe;

	if (restore_fpu_state_from_user_stack(&frame->uc.uc_mcontext))
		goto badframe;

        return regs->rax;
	
badframe:
	// signal_fault(regs, frame, "sigreturn");
printk("Bad Frame!\n");
	return 0;
}
