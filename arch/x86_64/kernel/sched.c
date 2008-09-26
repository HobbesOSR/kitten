#include <lwk/kernel.h>
#include <lwk/smp.h>
#include <lwk/task.h>
#include <arch/i387.h>

struct task_struct *
__arch_context_switch(struct task_struct *prev_p, struct task_struct *next_p)
{
	struct thread_struct *prev = &prev_p->arch.thread;
	struct thread_struct *next = &next_p->arch.thread;
	id_t cpu = cpu_id();
	struct tss_struct *tss = &per_cpu(tss, cpu);

	/* Update TSS */
	tss->rsp0 = next->rsp0;

	/* Switch DS and ES segment registers */
	asm volatile("mov %%es,%0" : "=m" (prev->es));
	if (unlikely(next->es | prev->es))
		loadsegment(es, next->es);
	asm volatile("mov %%ds,%0" : "=m" (prev->ds));
	if (unlikely(next->ds | prev->ds))
		loadsegment(ds, next->ds);

	/* Load FS and GS segment registers (used for thread local storage) */
	{
		unsigned int fsindex;
		asm volatile("movl %%fs,%0" : "=r" (fsindex));
		if (unlikely(fsindex | next->fsindex | prev->fs)) {
			loadsegment(fs, next->fsindex);
			if (fsindex)
				prev->fs = 0;
		}
		if (next->fs)
			wrmsrl(MSR_FS_BASE, next->fs);
		prev->fsindex = fsindex;
	}
	{
		unsigned int gsindex;
		asm volatile("movl %%gs,%0" : "=r" (gsindex));
		if (unlikely(gsindex | next->gsindex | prev->gs)) {
			load_gs_index(next->gsindex);
			if (gsindex)
				prev->gs = 0;
		}
		if (next->gs)
			wrmsrl(MSR_KERNEL_GS_BASE, next->gs);
		prev->gsindex = gsindex;
	}

	/* Update the CPU's PDA (per-CPU data area) */
	write_pda(pcurrent, next_p);

	/* If necessary, save and restore floating-point state */
	if (prev_p->arch.flags & TF_USED_FPU)
		fpu_save_state(prev_p);
	if (next_p->arch.flags & TF_USED_FPU) {
		fpu_restore_state(next_p);
		clts();
	} else {
		/*
		 * Set the CW flag of CR0 so that FPU/MMX/SSE instructions
		 * will cause a "Device not available" exception. The exception
		 * handler will then initialize the FPU state and set the
		 * task's TF_USED_FPU flag.  From that point on, the task
		 * should never experience another "Device not available"
		 * exception.
		 */
		stts();
	}

	return prev_p;
}

void
arch_idle_task_loop(void)
{
	while (1) {
		local_irq_enable();
		halt();
		local_irq_disable();
	}
}
