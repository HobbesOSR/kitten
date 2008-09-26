#include <lwk/kernel.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <arch/ptrace.h>

int
arch_task_create(struct task_struct *task,
                 const start_state_t *start_state)
{
	struct pt_regs *regs;
	kaddr_t kstack_top;
	kaddr_t initial_ksp;

	regs        = ((struct pt_regs *)((kaddr_t)task + TASK_SIZE)) - 1;
	kstack_top  = (kaddr_t)(regs + 1);
	initial_ksp = (kaddr_t)regs;

	task->arch.thread.rsp0    = kstack_top;
	task->arch.thread.rsp     = initial_ksp;
	task->arch.thread.userrsp = start_state->stack_ptr;

	/* Mark this as a new-task... arch_context_switch() checks this flag */
	task->arch.flags = TF_NEW_TASK;

	/* Task's address space is from [0, task->addr_limit) */
	task->arch.addr_limit = PAGE_OFFSET;

	/* Initialize FPU state */
	task->arch.thread.i387.fxsave.cwd = 0x37f;
	task->arch.thread.i387.fxsave.mxcsr = 0x1f80;

	/* CPU control unit uses these fields to start the user task running */
	if (start_state->aspace_id == KERNEL_ASPACE_ID) {
		regs->ss     = __KERNEL_DS;
		regs->rsp    = (vaddr_t)task + TASK_SIZE;
		regs->eflags = 0;
		regs->cs     = __KERNEL_CS;
	} else {
		regs->ss     = __USER_DS;
		regs->rsp    = start_state->stack_ptr;
		regs->eflags = (1 << 9);  /* enable interrupts */
		regs->cs     = __USER_CS;
	}
	regs->rip = start_state->entry_point;

	return 0;
}
