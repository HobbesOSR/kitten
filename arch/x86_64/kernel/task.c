#include <lwk/kernel.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <arch/ptrace.h>
#include <arch/prctl.h>
#include <arch/unistd.h>
#include <arch/i387.h>

int
arch_task_create(
	struct task_struct *	task,
	const start_state_t *	start_state,
	const struct pt_regs *	parent_regs
)
{
	/* Assume we entered via clone() syscall if parent_regs passed in */
	bool is_clone = (parent_regs != NULL);

	/*
	 * Put a pt_regs structure at the top of the new task's kernel stack.
	 * This is used by the CPU control unit and arch_context_switch()
	 * to setup the new task's register state. 
 	 */
	struct pt_regs *regs =
		((struct pt_regs *)((kaddr_t)task + TASK_SIZE)) - 1;

	/* Initialize GPRs */
	if (start_state->use_args) {
		/* Pass C-style arguments to new task */
		regs->rdi = start_state->arg[0];
		regs->rsi = start_state->arg[1];
		regs->rdx = start_state->arg[2];
		regs->rcx = start_state->arg[3];
	} else if (is_clone) {
		*regs = *parent_regs;
		regs->rax = 0;  /* set child's clone() return value to 0 */
	} else {
		/* Zero all registers */
		memset(regs, 0, sizeof(regs));
	}

	task->arch.thread.rsp0    = (kaddr_t)(regs + 1);    /* kstack top */
	task->arch.thread.rsp     = (kaddr_t)regs;          /* kstack ptr */
	task->arch.thread.userrsp = start_state->stack_ptr; /* ustack ptr */

	/* Mark this as a new-task... arch_context_switch() checks this flag */
	task->arch.flags = TF_NEW_TASK_MASK;

	/* Task's address space is from [0, task->addr_limit) */
	task->arch.addr_limit = PAGE_OFFSET;

	/* Initialize FPU state */
	reinit_fpu_state(task);

	/* If this is a clone, initialize new task's thread local storage */
	if (is_clone)
		do_arch_prctl(task, ARCH_SET_FS, parent_regs->r8);

	/* Initialize register state */
	if (start_state->aspace_id == KERNEL_ASPACE_ID) {
		regs->ss     = __KERNEL_DS;
		regs->rsp    = (vaddr_t)task + TASK_SIZE;
		regs->cs     = __KERNEL_CS;
	} else {
		regs->ss     = __USER_DS;
		regs->rsp    = start_state->stack_ptr;
		regs->cs     = __USER_CS;
	}
	regs->eflags = (1 << 9);  /* enable interrupts */
	regs->rip    = is_clone ? parent_regs->rip : start_state->entry_point;

	return 0;
}
