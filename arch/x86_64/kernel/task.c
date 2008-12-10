#include <lwk/kernel.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <arch/ptrace.h>

int
arch_task_create(struct task_struct *task,
                 const start_state_t *start_state)
{
	/*
	 * Put a pt_regs structure at the top of the new task's kernel stack.
	 * This is used by the CPU control unit and arch_context_switch()
	 * to setup the new task's register state. It is initialized below.
 	 */
	struct pt_regs *regs =
		((struct pt_regs *)((kaddr_t)task + TASK_SIZE)) - 1;

	task->arch.thread.rsp0    = (kaddr_t)(regs + 1);    /* kstack top */
	task->arch.thread.rsp     = (kaddr_t)regs;          /* kstack ptr */
	task->arch.thread.userrsp = start_state->stack_ptr; /* ustack ptr */

	/* Mark this as a new-task... arch_context_switch() checks this flag */
	task->arch.flags = TF_NEW_TASK;

	/* Task's address space is from [0, task->addr_limit) */
	task->arch.addr_limit = PAGE_OFFSET;

	/* Initialize FPU state */
	task->arch.thread.i387.fxsave.cwd = 0x37f;
	task->arch.thread.i387.fxsave.mxcsr = 0x1f80;

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
	regs->rip    = start_state->entry_point;
	regs->rdi    = start_state->arg[0];
	regs->rsi    = start_state->arg[1];
	regs->rdx    = start_state->arg[2];
	regs->rcx    = start_state->arg[3];

	return 0;
}
