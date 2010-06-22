/**
 * Architecture dependent entry points.
 *
 * Context switches, system calls, interrupts and other entry points
 * into the kernel that can not be implemented in straight C.
 */
#include <lwk/linkage.h>
#include <lwk/errno.h>
#include <arch/ptrace.h>
#include <arch/asm-offsets.h>
#include <arch/idt_vectors.h>
#include <arch/dwarf2.h>
#include <arch/task.h>
#include <arch/segment.h>


/**
 * Architecture-specific portion of a context switch.
 *
 * Normally, this is called in the context of prev and returns in the
 * context of next. However, new tasks are handled differently. Since
 * new tasks do not yet have a kernel context (rather, their kernel
 * stack just has the pt_regs to use for the new task), execution returns
 * directly to the new task, rather than context_switch().
 *
 * Returns prev in %rsi (same value as on entry)
 *
 * NOTE: External interrupts are disabled on entry.
 *
 * C Prototype:
 *       int arch_context_switch(struct task_struct *prev,
 *                               struct task_struct *next);
 */
ENTRY(arch_context_switch)
	/* Save prev's callee saved registers (others saved by caller) */
	pushf
	pushq %rbp
	pushq %rbx
	pushq %r12
	pushq %r13
	pushq %r14
	pushq %r15

	/* Switch to next's stack */
	movq %rsp, tsk_arch_rsp(%rdi)
	movq tsk_arch_rsp(%rsi), %rsp

	/* Call C code to do more stuff (save/restore FPU, update PDA, ...) */
	call __arch_context_switch
	/* returns with %rax set to prev */
	movq %rax, %rdi
	movq %gs:pda_pcurrent, %rsi

	/* New tasks need to be kick-started */
	lock btr $TF_NEW_TASK_BIT, tsk_arch_flags(%rsi)
	jc kickstart_new_task

	/* Restore next's callee saved registers */
	popq %r15
	popq %r14
	popq %r13
	popq %r12
	popq %rbx
	popq %rbp
	popf;

	/* Return to context_switch(), with new task active */
	retq

kickstart_new_task:
	call schedule_new_task_tail /* Finish up schedule(), drop locks, etc. */
	testl $3, CS(%rsp)          /* Sets ZF=1 if returning to kernel-space */
	je 1f                       /*     If ZF=1, leave kernel PDA in place */
	swapgs                      /* Install the user PDA                   */
1:
	movq     (%rsp), %r15       /* Unpack the pt_regs struct that         */
	movq  1*8(%rsp), %r14       /* arch_task_create() put at the top      */
	movq  2*8(%rsp), %r13       /* of the new task's kernel stack.        */
	movq  3*8(%rsp), %r12
	movq  4*8(%rsp), %rbp
	movq  5*8(%rsp), %rbx
	movq  6*8(%rsp), %r11
	movq  7*8(%rsp), %r10
	movq  8*8(%rsp), %r9
	movq  9*8(%rsp), %r8
	movq 10*8(%rsp), %rax
	movq 11*8(%rsp), %rcx
	movq 12*8(%rsp), %rdx
	movq 13*8(%rsp), %rsi
	movq 14*8(%rsp), %rdi
	addq $16*8, %rsp            /* Move RSP to RIP slot in pt_regs   */
	iretq                       /* Start the new task running             */

END(arch_context_switch)


/**
 * Entry point for system calls.
 *
 * Upon entry we are still running with the user-level stack and the
 * x86_64 CPU control unit has stashed the user-level RIP in RCX
 * and RFLAGS in R11.
 *
 * The first thing this function does is generate a partial stack frame
 * containing all caller-saved registers. After this is done, the system call
 * number (stored in RAX by user-level) is used to index into the system call
 * table (sys_call_table) and call the handler function. The handler function
 * is responsible for saving all callee-saved registers. If it is a C
 * function, callee-saved registers are saved automatically by the compiler.
 *
 * Immediately before calling the syscall handler function, the kernel stack
 * looks like the following.  All fields represent the saved state of the
 * calling user-level task:
 *
 *            UNDEF      = Undefined, normally holds user-space SS
 *            UNDEF      = Undefined, normally holds user-space RSP
 *            UNDEF      = Undefined, normally holds user-space RFLAGS
 *            UNDEF      = Undefined, normally holds user-space CS
 *            RIP        = user-space RIP
 *            ORIG_RAX   = system call number, passed from user-space
 *            RDI        = ARG0, passed from user-space
 *            RSI        = ARG1, passed from user-space
 *            RDX        = ARG2, passed from user-space
 *            (junk)     = normally RCX, but RCX is clobbered by SYSCALL
 *            RAX        = system call number, passed from user-space
 *            R8         = ARG4, passed from user-space
 *            R9         = ARG5, passed from user-space
 *            R10        = ARG3, passed from user-space
 *     RSP -> R11        = user-space RFLAGS
 *
 * And the registers are setup as follows:
 *
 *            RDI        = ARG0
 *            RSI        = ARG1
 *            RDX        = ARG2
 *            RCX        = ARG3 (was stored on R10 on entry)
 *            R8         = ARG4
 *            R9         = ARG5
 *            RAX        = System call number
 *
 * NOTE: RCX and R11 are clobbered by system calls. This is due to the SYSCALL
 *       instruction using RCX and R11 to store RIP and RFLAGS before
 *       transfering control to the kernel. User-level will observe different
 *       values of RCX and R11 after SYSCALL than before.
 * 
 * NOTE: External interrupts are disabled on entry.
 *
 * C Prototype:
 *       int asm_syscall(void);
 */
ENTRY(asm_syscall)
	/*
	 * Enter from user-space
	 */
	swapgs				/* Load GS.base with kernel PDA addr */
	movq %rsp, %gs:pda_oldrsp	/* Backup user-space RSP             */
	movq %gs:pda_kernelstack, %rsp	/* Load kernel stack                 */

	/*
	 * Save registers to kernel-stack
	 */
	subq $10*8, %rsp		/* Make room on the stack            */
	movq %rcx, 10*8(%rsp)		/* Save user-space RIP               */
	movq %rax,  9*8(%rsp)		/* Save syscall # in ORIG_RAX slot   */
	movq %rdi,  8*8(%rsp)		/* Save user-space RDI (ARG0)        */
	movq %rsi,  7*8(%rsp)		/* Save user-space RSI (ARG1)        */
	movq %rdx,  6*8(%rsp)		/* Save user-space RDX (ARG2)        */
	movq %rcx,  5*8(%rsp)		/* RCX is clobbered, save anyways    */
	movq %rax,  4*8(%rsp)		/* Save user-space RAX (syscall #)   */
	movq %r8,   3*8(%rsp)		/* Save user-space R8  (ARG4)        */
	movq %r9,   2*8(%rsp)		/* Save user-space R9  (ARG5)        */
	movq %r10,  1*8(%rsp)		/* Save user-space R10 (ARG3)        */
	movq %r11,     (%rsp)		/* Save user-space RFLAGS            */

	/*
	 * Call the system call handler
	 */
	movq %r10, %rcx			/* Per x86_64 C ABI, RCX holds ARG3  */
	cmp $__NR_syscall_max, %rax	/* Make sure syscall # is in range   */
	jg 1f
	sti				/* Enable external interrupts        */
	call *sys_call_table(,%rax,8)	/* Call the system call handler      */
	cli				/* Disable external interrupts       */
	jmp 2f
1:
	call syscall_not_implemented	/* Print error and return            */
2:
	movq %rax, 4*8(%rsp)		/* Save return code in stack frame   */

	/*
	 * Fast-path optimization, if there is no extra-work to do,
	 * jump straight to the return to user-space code.
	 */
	movq %gs:pda_pcurrent, %rdi	/* Move current task pointer to RDI  */
	movl tsk_arch_flags(%rdi), %esi /* Move current->arch.flags to ESI   */
	testl %esi, %esi		/* Sets zero flag if ESI=0           */
	jz 4f				/* If zero flag is set, goto 4:      */

	/*
	 * Reschedule if necessary
	 */
	testl $TF_NEED_RESCHED_MASK,%esi /* AND TF_NEED_RESCHED_MASK with ESI */
	jz 3f				/* If result is zero, goto 3:        */
	pushq %rsi			/* Backup RSI to stack               */
	call schedule			/* Try to reschedule                 */
	popq %rsi			/* Restore RSI from stack            */
3:
	/*
	 * Handle pending signals, if any
	 */
	testl $TF_SIG_PENDING_MASK,%esi /* AND TF_SIG_PENDING_MASK with ESI  */
	jz 4f				/* If result is zero, goto 4:        */
	jmp asm_syscall_do_signal       /* Try to handle pending signals     */
4:
	/*
	 * Return to user-space
	 */
	movq     (%rsp), %r11		/* Restore RFLAGS for SYSRET         */
	movq  1*8(%rsp), %r10		/* Restore user-space R10 (ARG3)     */
	movq  2*8(%rsp), %r9		/* Restore user-space R9  (ARG5)     */
	movq  3*8(%rsp), %r8		/* Restore user-space R8  (ARG4)     */
	movq  4*8(%rsp), %rax		/* Return syscall return code        */
	movq  6*8(%rsp), %rdx		/* Restore user-space RDX (ARG2)     */
	movq  7*8(%rsp), %rsi		/* Restore user-space RSI (ARG1)     */
	movq  8*8(%rsp), %rdi		/* Restore user-space RDI (ARG0)     */
	movq 10*8(%rsp), %rcx		/* Restore RIP for SYSRET            */
	movq %gs:pda_oldrsp, %rsp	/* Restore user-space RSP            */
	swapgs				/* Restore user-space GS.base        */
	sysretq				/* Return to user-space              */
END(asm_syscall)


/*
 * Wrapper that constructs a complete pt_regs structure on the kernel
 * stack, and then calls do_signal() to handle any pending signals.
 *
 * NOTE: External interrupts are disabled on entry.
 */
ENTRY(asm_syscall_do_signal)
	subq $6*8, %rsp			/* Make room for rest of user regs   */
	movq %rbx, 5*8(%rsp)		/* Save user-space RBX               */
	movq %rbp, 4*8(%rsp)		/* Save user-space RBP               */
	movq %r12, 3*8(%rsp)		/* Save user-space R12               */
	movq %r13, 2*8(%rsp)		/* Save user-space R13               */
	movq %r14, 1*8(%rsp)		/* Save user-space R14               */
	movq %r15,    (%rsp)		/* Save user-space R15               */

	movq $(__USER_DS),   20*8(%rsp)	/* Write user-space SS to pt_regs    */
	movq %gs:pda_oldrsp, %r11
	movq %r11,           19*8(%rsp)	/* Write user-space RSP to pt_regs   */
	movq 6*8(%rsp),      %r11
	movq %r11,           18*8(%rsp)	/* Write user-space RFLAGS to pt_regs*/
	movq $(__USER_CS),   17*8(%rsp)	/* Write user-space CS to pt_regs    */

	/* There is now a complete ptregs structure on the stack */

	movq %rsp, %rdi			/* Pass ptr to pt_regs in RDI (ARG0) */
	call do_signal			/* Handle pending signals, if any    */

	/*
	 * Return to user-space using the slower iretq mechanism
	 * so that all registers in ptregs can be restored -- sysretq
	 * clobbers RCX and R11.
	 */
	swapgs				/* Restore user-space GS register    */
	movq     (%rsp), %r15		/* Restore user-space R15            */
	movq  1*8(%rsp), %r14		/* Restore user-space R14            */
	movq  2*8(%rsp), %r13		/* Restore user-space R13            */
	movq  3*8(%rsp), %r12		/* Restore user-space R12            */
	movq  4*8(%rsp), %rbp		/* Restore user-space RBP            */
	movq  5*8(%rsp), %rbx		/* Restore user-space RBX            */
	movq  6*8(%rsp), %r11		/* Restore user-space R11            */
	movq  7*8(%rsp), %r10		/* Restore user-space R10            */
	movq  8*8(%rsp), %r9		/* Restore user-space R9             */
	movq  9*8(%rsp), %r8		/* Restore user-space R8             */
	movq 10*8(%rsp), %rax		/* Restore user-space RAX            */
	movq 11*8(%rsp), %rcx		/* Restore user-space RCX            */
	movq 12*8(%rsp), %rdx		/* Restore user-space RDX            */
	movq 13*8(%rsp), %rsi		/* Restore user-space RSI            */
	movq 14*8(%rsp), %rdi		/* Restore user-space RDI            */
	addq $16*8, %rsp		/* Move RSP to RIP slot in pt_regs   */
	iretq
END(asm_syscall_do_signal)


/*
 * Wrapper that constructs a complete pt_regs structure on the kernel
 * stack, and then calls sys_rt_sigreturn().
 *
 * NOTE: External interrupts are ENABLED on entry.
 *
 * C Prototype:
 *       int asm_sys_rt_sigreturn(void);
 */
ENTRY(asm_sys_rt_sigreturn)
	cli				/* Disable external interrupts       */
	popq %r11			/* Pop return address off stack      */

	subq $6*8, %rsp			/* Make room for rest of user regs   */
	movq %rbx, 5*8(%rsp)		/* Save user-space RBX               */
	movq %rbp, 4*8(%rsp)		/* Save user-space RBP               */
	movq %r12, 3*8(%rsp)		/* Save user-space R12               */
	movq %r13, 2*8(%rsp)		/* Save user-space R13               */
	movq %r14, 1*8(%rsp)		/* Save user-space R14               */
	movq %r15,    (%rsp)		/* Save user-space R15               */

	movq $(__USER_DS),   20*8(%rsp)	/* Write user-space SS to pt_regs    */
	movq %gs:pda_oldrsp, %r11
	movq %r11,           19*8(%rsp)	/* Write user-space RSP to pt_regs   */
	movq 6*8(%rsp),      %r11
	movq %r11,           18*8(%rsp)	/* Write user-space RFLAGS to pt_regs*/
	movq $(__USER_CS),   17*8(%rsp)	/* Write user-space CS to pt_regs    */

	/* There is now a complete ptregs structure on the stack */

	movq %rsp, %rdi			/* Pass ptr to pt_regs in RDI (ARG0) */
	sti				/* Enable external interrupts        */
	call sys_rt_sigreturn		/* Call sys_rt_sigreturn()           */
	cli				/* Disable external interrupts       */

	/*
	 * Fast-path optimization, if there is no extra-work to do,
	 * jump straight to the return to user-space code.
	 */
	movq %gs:pda_pcurrent, %r12	/* Move current task pointer to R12  */
	movl tsk_arch_flags(%r12), %ebx /* Move current->arch.flags to EBX   */
	testl %ebx, %ebx		/* Sets zero flag if EBX=0           */
	jz 2f				/* If zero flag is set, goto 2:      */

	/*
	 * Reschedule if necessary
	 */
	testl $TF_NEED_RESCHED_MASK,%ebx /* AND TF_NEED_RESCHED_MASK with EBX */
	jz 1f				/* If result is zero, goto 1:        */
	call schedule			/* Try to reschedule                 */
1:
	/*
	 * Handle pending signals, if any
	 */
	testl $TF_SIG_PENDING_MASK,%ebx /* AND TF_SIG_PENDING_MASK with EBX  */
	jz 2f				/* If result is zero, goto 2:        */
	movq %rsp, %rdi			/* Pass ptr to pt_regs in RDI (ARG0) */
	call do_signal			/* Handle pending signals, if any    */
2:
	/*
	 * Return to user-space using the slower iretq mechanism
	 * so that all registers in ptregs can be restored -- sysretq
	 * clobbers RCX and R11.
	 */
	swapgs				/* Restore user-space GS register    */
	movq     (%rsp), %r15		/* Restore user-space R15            */
	movq  1*8(%rsp), %r14		/* Restore user-space R14            */
	movq  2*8(%rsp), %r13		/* Restore user-space R13            */
	movq  3*8(%rsp), %r12		/* Restore user-space R12            */
	movq  4*8(%rsp), %rbp		/* Restore user-space RBP            */
	movq  5*8(%rsp), %rbx		/* Restore user-space RBX            */
	movq  6*8(%rsp), %r11		/* Restore user-space R11            */
	movq  7*8(%rsp), %r10		/* Restore user-space R10            */
	movq  8*8(%rsp), %r9		/* Restore user-space R9             */
	movq  9*8(%rsp), %r8		/* Restore user-space R8             */
	movq 10*8(%rsp), %rax		/* Restore user-space RAX            */
	movq 11*8(%rsp), %rcx		/* Restore user-space RCX            */
	movq 12*8(%rsp), %rdx		/* Restore user-space RDX            */
	movq 13*8(%rsp), %rsi		/* Restore user-space RSI            */
	movq 14*8(%rsp), %rdi		/* Restore user-space RDI            */
	addq $16*8, %rsp		/* Move RSP to RIP slot in pt_regs   */
	iretq
END(asm_sys_rt_sigreturn)


/**
 * Initial handler for clone() system calls.
 *
 * It backs up the registers that haven't already been saved
 * by asm_syscall() and then passes a pointer to the complete
 * 'struct pt_regs' to sys_clone().
 *
 * C Prototype:
 *       int asm_sys_clone(void);
 */
ENTRY(asm_sys_clone)
	popq %r11			/* Pop return address off stack      */

	subq $6*8, %rsp			/* Make room for rest of user regs   */
	movq %rbx, 5*8(%rsp)		/* Save user-space RBX               */
	movq %rbp, 4*8(%rsp)		/* Save user-space RBP               */
	movq %r12, 3*8(%rsp)		/* Save user-space R12               */
	movq %r13, 2*8(%rsp)		/* Save user-space R13               */
	movq %r14, 1*8(%rsp)		/* Save user-space R14               */
	movq %r15,    (%rsp)		/* Save user-space R15               */

	movq %r11, %r15			/* Stash retaddr in callee saved reg */
	movq %rsp, %r8			/* Pass ptr to pt_regs in R8 (ARG4)  */
	call sys_clone			/* Call sys_clone()                  */
	movq %r15, %r11			/* Unstash asm_syscall return addr   */

	movq    (%rsp), %r15		/* Restore user-space R15            */
	movq 1*8(%rsp), %r14		/* Restore user-space R14            */
	movq 2*8(%rsp), %r13		/* Restore user-space R13            */
	movq 3*8(%rsp), %r12		/* Restore user-space R12            */
	movq 4*8(%rsp), %rbp		/* Restore user-space RBP            */
	movq 5*8(%rsp), %rbx		/* Restore user-space RBX            */
	addq $6*8, %rsp			/* Restore RSP asm_syscall expects   */

	pushq %r11			/* Push return address onto stack    */
	ret				/* Return to asm_syscall             */
END(asm_sys_clone)



/**
 * Handler for SYSCALL instructions issued from compatibility mode.
 * 
 * NOTE: We don't support them.
 *
 * C Prototype:
 *       int asm_syscall_ignore(void);
 */
ENTRY(asm_syscall_ignore)
	mov $-ENOSYS,%eax
	sysret
END(asm_syscall_ignore)


/**
 * Common entry point for all interrupts. 
 * 
 * Immediately before calling the interrupt handler function, the kernel stack
 * looks like the following.  All fields represent the state of the interrupted
 * context of execution, immediately before it was interrupted:
 *
 *            [...]
 *            SS         = Stack Segment Selector
 *            RSP        = Stack Pointer
 *            RFLAGS     = Flags Register
 *            CS         = Code Segment Selector
 *            RIP        = Instruction Pointer
 *            ERROR_CODE = Error Code, 0 for interrupts with no error code
 *            RDI        = Vector #
 *            RSI
 *            RDX
 *            RCX
 *            RAX
 *            R8
 *            R9
 *            R10
 *            R11
 *            RBX
 *            RBP
 *            R12
 *            R13
 *            R14
 *     RSP -> R15
 *
 * And the registers are setup as follows according to the C calling
 * convention for the C function do_interrupt() that will vector
 * the interrupt to the appropriate registered handler.
 *
 *            RDI        = ARG0: A fully populated 'struct pt_regs *'
 *            RSI        = ARG1: The interrupt vector number 
 *
 * NOTE: External interrupts are disabled on entry.
 *
 * C Prototype:
 *       void asm_interrupt(void);
 */
ENTRY(asm_interrupt)
	cld				/* Clear direction flag              */

	/*
	 * Save registers to kernel-stack
	 */
	subq $14*8, %rsp		/* Make room on the stack            */
	movq %rsi, 13*8(%rsp)
	movq 14*8(%rsp), %rsi		/* ARG1: the interrupt vector number */
	movq %rdi, 14*8(%rsp)
	movq %rdx, 12*8(%rsp)
	movq %rcx, 11*8(%rsp)
	movq %rax, 10*8(%rsp)
	movq %r8,   9*8(%rsp)
	movq %r9,   8*8(%rsp)
	movq %r10,  7*8(%rsp) 
	movq %r11,  6*8(%rsp) 
	movq %rbx,  5*8(%rsp)
	movq %rbp,  4*8(%rsp)
	movq %r12,  3*8(%rsp)
	movq %r13,  2*8(%rsp)
	movq %r14,  1*8(%rsp)
	movq %r15,     (%rsp)

	/*
	 * Load kernel GS if we're coming from user-space
	 */
	testl $3, CS(%rsp)		/* Sets ZF=1 if coming from kspace   */
	je 1f				/* If ZF=1, skip installing the PDA  */
	swapgs				/* Install the PDA                   */
1:	
	/*
	 * Call C code interrupt handler entry point
	 */
	incl %gs:pda_irqcount		/* Mark that we're in an interrupt   */
	movq %rsp, %rdi			/* ARG0: pointer to 'struct pt_regs' */
	call do_interrupt		/* Call common C handler             */
	decl %gs:pda_irqcount		/* Unmark that we're in an interrupt */

	/*
	 * If returning to kernel-space, skip straight to 4: below
	 */
	testl $3, CS(%rsp)		/* Sets ZF=1 if returning to kspace  */
	je 4f				/* If ZF=1, jump forward to 4: below */

	/*
	 * Fast-path optimization, if there is no extra-work to do,
	 * jump straight to the return to user-space code.
	 */
	movq %gs:pda_pcurrent, %r12	/* Move current task pointer to R12  */
	movl tsk_arch_flags(%r12), %ebx /* Move current->arch.flags to EBX   */
	testl %ebx, %ebx		/* Sets zero flag if EBX=0           */
	jz 3f				/* If zero flag is set, goto 3:      */

	/*
	 * Reschedule if necessary
	 */
	testl $TF_NEED_RESCHED_MASK,%ebx /* AND TF_NEED_RESCHED_MASK with EBX */
	jz 2f				/* If result is zero, goto 2:        */
	call schedule			/* Try to reschedule                 */
2:
	/*
	 * Handle pending signals, if any
	 */
	testl $TF_SIG_PENDING_MASK,%ebx	/* AND TF_SIG_PENDING_MASK with EBX  */
	jz 3f				/* If result is zero, goto 3:        */
	movq %rsp, %rdi			/* Pass ptr to pt_regs in RDI (ARG0) */
	call do_signal			/* Handle pending signals, if any    */
3:
	swapgs				/* Restore user-space GS register    */
4:
	/*
	 * Restore registers and return to interrupted program
	 */
	movq     (%rsp), %r15
	movq  1*8(%rsp), %r14
	movq  2*8(%rsp), %r13
	movq  3*8(%rsp), %r12
	movq  4*8(%rsp), %rbp
	movq  5*8(%rsp), %rbx
	movq  6*8(%rsp), %r11
	movq  7*8(%rsp), %r10
	movq  8*8(%rsp), %r9
	movq  9*8(%rsp), %r8
	movq 10*8(%rsp), %rax
	movq 11*8(%rsp), %rcx
	movq 12*8(%rsp), %rdx
	movq 13*8(%rsp), %rsi
	movq 14*8(%rsp), %rdi
	addq $16*8, %rsp
	iretq
END(asm_interrupt)


/**
 * This table contains an initial entry point function for each IDT vector.
 * When an interrupt vector fires, the first instruction executed is at
 * table[vector].
 *
 * This table scheme is necessary because some x86_64 interrupts push an
 * error code onto the stack and others do not. Additionally, there is no way
 * for an interrupt handler to determine the interrupt vector that triggered
 * it. Therefore, the functions in this table push a dummy error code onto
 * the stack when necessary, always push the vector number, and then call
 * the common handler asm_interrupt().
 *
 * WARNING: Each function/entry in this table must be <= 16 bytes.
 *          Be very careful when adding instructions.
 * 
 * C Prototype:
 *       void asm_idtvec_table(void) { asm_interrupt(); }
 */
.align 16
ENTRY(asm_idtvec_table)
	vector=0
	.rept NUM_IDT_ENTRIES
		.if vector<=7||vector==9||vector==15||vector==16||vector>=18
		pushq $0		/* push dummy error_code */
		.endif
		pushq $vector		/* push vector # into RDI slot */
		jmp asm_interrupt	/* call common handler */

		/* Move onto next entry in table*/
		.align 16
		vector=vector+1
	.endr
END(asm_idtvec_table)


/**
 * Reload gs selector with exception handling.
 *     edi:  new selector
 *
 * C Prototype:
 *       void load_gs_index(unsigned gs);
 */
ENTRY(load_gs_index)
	CFI_STARTPROC
	pushf
	CFI_ADJUST_CFA_OFFSET 8
	cli
	swapgs
gs_change:
	movl %edi,%gs
2:	mfence          /* workaround */
	swapgs
	popf
	CFI_ADJUST_CFA_OFFSET -8
	ret
	CFI_ENDPROC
ENDPROC(load_gs_index)

	.section __ex_table,"a"
	.align 8
	.quad gs_change,bad_gs
	.previous
	.section .fixup,"ax"
	/* running with kernelgs */
bad_gs:
	swapgs                  /* switch back to user gs */
	xorl %eax,%eax
	movl %eax,%gs
	jmp  2b
	.previous

