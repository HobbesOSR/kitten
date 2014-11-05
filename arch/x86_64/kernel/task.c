#include <lwk/kernel.h>
#include <lwk/aspace.h>
#include <lwk/task.h>
#include <arch/ptrace.h>
#include <arch/prctl.h>
#include <arch/unistd.h>
#include <arch/i387.h>

#ifdef CONFIG_TASK_MEAS
#define MSR_RAPL_POWER_UNIT    0x606

#define MSR_POWER_LIMIT        0x610
#define MSR_ENERGY_STATUS      0x611
#define MSR_PERF_STATUS        0x613
#define MSR_POWER_INFO         0x614

static inline int wrmsr_eio(u32 reg, u64 val)
{
	u32 eax = (u32)val;
	u32 edx = val >> 32;
	int err;

	asm volatile(
	       "1:	wrmsr\n"
	       "2:\n"
	       ".section .fixup,\"ax\"\n"
	       "3:	movl %4,%0\n"
	       "	jmp 2b\n"
	       ".previous\n"
	       ".section __ex_table,\"a\"\n"
	       "	.align 8\n"
	       "	.quad 1b,3b\n"
	       ".previous"
	       : "=&bDS" (err)
	       : "a" (eax), "d" (edx), "c" (reg), "i" (-EIO), "0" (0));

	return err;
}

static inline int rdmsr_eio(u32 reg, u64 *val)
{
	int err;
	u32 eax, edx;

	asm volatile(
	       "1:	rdmsr\n"
	       "2:\n"
	       ".section .fixup,\"ax\"\n"
	       "3:	movl %4,%0\n"
	       "	jmp 2b\n"
	       ".previous\n"
	       ".section __ex_table,\"a\"\n"
	       "	.align 8\n"
	       "	.quad 1b,3b\n"
	       ".previous"
	       : "=&bDS" (err), "=a" (eax), "=d" (edx)
	       : "c" (reg), "i" (-EIO), "0" (0));

	*val = eax;
	*val |= ((u64)edx << 32);

	return err;
}

static inline int rdmsr_units(struct raplunits *units)
{
	u64 val;
	int err = 0;

	if( !rdmsr_eio(MSR_RAPL_POWER_UNIT, &val) )
	{
		units->power = (u16)(val & 0xf);
		units->energy = (u16)(val >> 8 & 0x1f);
		units->time = (u16)(val >> 16 & 0xf);
		units->set = 1;
#ifdef CONFIG_TASK_MEAS_DEBUG
		printk( "MEASUREMENT UNITS - time=%llu energy=%llu, power=%llu\n",
			units->time, units->energy, units->power );
#endif
	}
	else
	{
		printk( "ERROR: Failed MSR I/O read\n" );
		err = -1;
	}

	return err;
}

void
arch_task_meas_start(void)
{
	u64 val = 0;

	if( !rdmsr_eio(MSR_ENERGY_STATUS, &val) )
	{
		current->meas.start_time = get_time();
		current->meas.start_energy = val;
	}
}

void
arch_task_meas(void)
{
	u64 val = 0;

	if( !rdmsr_eio(MSR_ENERGY_STATUS, &val) )
	{
		if( current->meas.start_time )
		{
			current->meas.time += (get_time() - current->meas.start_time);
			if( val < current->meas.start_energy )
			{
				current->meas.energy +=
					(((unsigned long long)(1)<<32) -
					 current->meas.start_energy + val);
			}
			else
			{
				current->meas.energy += (val - current->meas.start_energy);
			}
#ifdef CONFIG_TASK_MEAS_DEBUG
			printk( "MEASUREMENT (%s, %u, %u) - count=%llu time=%llu energy=%llu, power=%llu\n",
				(char *)current->name, (unsigned int)current->aspace->id, (unsigned int)current->id,
				current->meas.count, current->meas.time, current->meas.energy,
				(current->meas.energy / (current->meas.time/1000000000)) );
#endif
		}
		current->meas.start_time = get_time();
		current->meas.start_energy = val;

		if( !current->meas.units.set && rdmsr_units( &(current->meas.units) ) )
			printk( "ERROR: Failed to retrieve MSR units\n" );
	}
}
#endif

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
		memset(regs, 0, sizeof(struct pt_regs));
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
