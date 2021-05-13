#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/delay.h>
#include <lwk/bootmem.h>
#include <lwk/task.h>
#include <lwk/sched.h>
#include <arch/atomic.h>

/**
 * MP boot trampoline 80x86 program as an array.
 */
extern unsigned char trampoline_data[];
extern unsigned char trampoline_end[];

/**
 * These specify the initial stack pointer and instruction pointer for a
 * newly booted CPU.
 */
//extern volatile unsigned long init_rsp;
extern void (*initial_code)(void);

void __init
start_secondary(void)
{
	cpu_init();
	cpu_set(this_cpu, cpu_online_map);


//	lapic_set_timer_freq(sched_hz);
	printk("TODO: Set the periodic timer frequency\n");

	schedule(); /* runs idle_task, since that's the only task
	             * on the CPU's run queue at this point */
}

void __init
arch_boot_cpu(unsigned int cpu)
{
#if 0
	union task_union *new_task_union;
	struct task_struct *new_task;

	/*
	 * Setup the 'trampoline' cpu boot code. The trampoline contains the
	 * first code executed by the CPU being booted. x86 CPUs boot in
	 * pre-historic 16-bit 'real mode'... the trampoline does the messy
	 * work to get us to 64-bit long mode and then calls the *initial_code
	 * kernel entry function.
	 */
	memcpy(__va(SMP_TRAMPOLINE_BASE), trampoline_data,
	                                  trampoline_end - trampoline_data
	);

	/*
	 * Allocate memory for the new CPU's GDT.
	 */
	//cpu_gdt_descr[cpu].address = (unsigned long) kmem_get_pages(0);

	/*
	 * Allocate memory for the new CPU's bootstrap task.
	 */
	new_task_union = kmem_get_pages(TASK_ORDER);
	new_task = &new_task_union->task_info;

	/*
	 * Initialize the bare minimum info needed to bootstrap the new CPU.
	 */
	new_task->id      = 0;
	new_task->aspace  = &bootstrap_aspace;
	new_task->cpu_id  = cpu;
	strcpy(new_task->name, "bootstrap");
	list_head_init(&new_task->sched_link);

	/*
	 * Set the initial kernel entry point and stack pointer for the new CPU.
	 */
	initial_code = start_secondary;
	// TODO Brian Will need to address this issue while working on MP boot.
	/*
	init_rsp     = (unsigned long)new_task_union
	                                  + sizeof(union task_union) - 1;
	 */
	/*
	 * Boot it!
	 */
	lapic_send_init_ipi(cpu);
	lapic_send_startup_ipi(cpu, SMP_TRAMPOLINE_BASE);
	lapic_send_startup_ipi(cpu, SMP_TRAMPOLINE_BASE);
#endif
}

