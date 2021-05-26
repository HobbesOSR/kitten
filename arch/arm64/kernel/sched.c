#include <lwk/kernel.h>
#include <lwk/smp.h>
#include <lwk/task.h>
#include <arch/fpsimd.h>

struct task_struct *
__arch_context_switch(struct task_struct *prev_p, struct task_struct *next_p)
{
	struct thread_struct *prev = &prev_p->arch.thread;
	struct thread_struct *next = &next_p->arch.thread;
	id_t cpu = this_cpu;


	printk("prev_p = %p, next_p = %p\n", prev_p, next_p);

	printk("NExt TP_Value=%p\n", next->tp_value);

	/* Load the TPIDR_EL0 for User TLS */
	{
		prev->tp_value = __mrs(tpidr_el0);
		__msr(tpidr_el0, next->tp_value);
	}

	/* Update the CPU's PDA (per-CPU data area) */
	prev->cpu_context.usersp = read_pda(oldsp);
	write_pda(oldsp, (next->cpu_context.usersp));
	write_pda(pcurrent, next_p);
	write_pda(kernelstack, (vaddr_t)next_p + TASK_SIZE - PDA_STACKOFFSET);

	/* save and restore floating-point state */
	{
		struct fpsimd_state * next_fpsimd = &next->fpsimd_state;
		struct fpsimd_state * prev_fpsimd = &prev->fpsimd_state;

		fpsimd_save_state(prev_fpsimd);
		fpsimd_load_state(next_fpsimd);
	}

	return prev_p;
}

void
arch_idle_task_loop_body(int irqenable)
{
	/* Issue HALT instruction,
	 * which should put CPU in a lower power mode */
	if (irqenable) {
		safe_halt();
	} else {
		halt();
	}
}
