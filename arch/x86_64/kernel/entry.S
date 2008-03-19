#include <lwk/linkage.h>
#include <lwk/errno.h>
#include <arch/ptrace.h>
#include <arch/asm-offsets.h>
#include <arch/idt_vectors.h>
#include <arch/dwarf2.h>


/**
 * This starts a task executing.
 * extern void ATTRIB_NORET asm_run_task(struct pt_regs *context)
 */
ENTRY(asm_run_task)
	swapgs			/* Install the user PDA */
	mov $0, %rdx		/* crt1 expects this to be addr of rtld_fini */
	pop %rax		/* Pop the return address */
	mov %rdi, %rsp		/* Point stack pointer at pt_regs input arg */
	add $128, %rsp		/* Bump to point to the RIP slot in pt_regs */
	iretq			/* Start the task executing */
END(asm_run_task)


/**
 * This is the entry point for system calls. Upon entry we are still running
 * with the user-level stack and the x86_64 CPU control unit has stashed the
 * user-level RIP in RCX and RFLAGS in R11. External interrupts are diabled.
 *
 * The first thing this function does is generate a partial stack frame
 * containing all caller-saved registers. After this is done, the system call
 * number (stored in RAX by user-level) is used to index into the system call
 * table (sys_call_table) and call the handler function. The handler function
 * is responsible for saving all callee-saved registers... if it is a C
 * function, callee-saved registers are saved automatically by the compiler.
 *
 * Immediately before calling the handler function, the kernel stack looks
 * like:
 *
 *            RIP      = user-space RIP
 *            ORIG_RAX = system call number, passed from user-space
 *            RDI      = ARG0, passed from user-space
 *            RSI      = ARG1, passed from user-space
 *            RDX      = ARG2, passed from user-space
 *            (junk)   = normally RCX, but RCX is clobbered by SYSCALL
 *            RAX      = system call number, passed from user-space
 *            R8       = ARG4, passed from user-space
 *            R9       = ARG5, passed from user-space
 *            R10      = ARG3, passed from user-space
 *     RSP -> R11      = user-space RFLAGS
 *
 * And the registers are setup as follows:
 *
 *            RDI      = ARG0
 *            RSI      = ARG1
 *            RDX      = ARG2
 *            RCX      = ARG3 (was stored on R10 on entry)
 *            R8       = ARG4
 *            R9       = ARG5
 *            RAX      = System call number
 *
 * NOTE: RCX and R11 are clobbered by system calls. This is due to the SYSCALL
 *       instruction using RCX and R11 to store RIP and RFLAGS before
 *       transfering control to the kernel. User-level will observe different
 *       values of RCX and R11 after SYSCALL than before.
 * 
 * NOTE: External interrupts are disabled on entry.
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
	movq %r11,     (%rsp)		/* R11 is clobbered, save anyways    */

	/*
	 * Call the system call handler
	 */
	movq %r10, %rcx			/* Per x86_64 C ABI, RCX holds ARG3  */
	cmp $__NR_syscall_max, %rax	/* Make sure syscall # is in range   */
	jg 1f
	call *sys_call_table(,%rax,8)	/* Call the system call handler      */
	jmp 2f
1:
	call syscall_not_implemented	/* Print error and return            */
2:
	movq %rax, 4*8(%rsp)		/* Save return code in stack frame   */

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


/**
 * This is a handler for SYSCALL instructions issued from compatibility mode...
 * we don't support them.
 */
ENTRY(asm_syscall_ignore)
	mov $-ENOSYS,%eax
	sysret
END(asm_syscall_ignore)


/**
 * This is the common entry point for all interrupts. 
 * 
 * Before calling the C handler function, the kernel stack looks like:
 *
 *            [...]
 *            SS         (stack segment selector)
 *            RSP        (stack pointer)
 *            RFLAGS     (flags register)
 *            CS         (code segment selector)
 *            RIP        (instruction pointer)
 *            ERROR_CODE (0 for interrupts with no error code)
 *            RDI        (this was the vector # on entry, we move to %rsi/ARG1)
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
 * And the registers are setup as follows:
 *
 *            RDI      = ARG0: A fully populated 'struct pt_regs *'
 *            RSI      = ARG1: The interrupt vector number 
 *
 * NOTE: External interrupts are disabled on entry.
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
	movq %rsp, %rdi			/* ARG0: pointer to 'struct pt_regs' */
	call do_interrupt		/* Call common C handler             */

	/*
	 * If returning to user-space, restore user-space GS
	 */
	testl $3, CS(%rsp)		/* Sets ZF=1 if returning to kspace  */
	je 2f				/* If ZF=1, leave PDA in place       */
	swapgs				/* Restore uspace GS register        */
2:
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
 * the stack when necessary, always push the vector number, and then call a
 * common handler (asm_interrupt).
 *
 * WARNING: Each function/entry in this table must be <= 16 bytes.
 *          Be very careful when adding instructions.
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


