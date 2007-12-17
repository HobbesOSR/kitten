#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/delay.h>
#include <lwk/bootmem.h>
#include <lwk/task.h>
#include <arch/atomic.h>
#include <arch/apicdef.h>
#include <arch/apic.h>
#include <arch/desc.h>

/**
 * MP boot trampoline 80x86 program as an array.
 */
extern unsigned char trampoline_data[];
extern unsigned char trampoline_end[];

/**
 * These specify the initial stack pointer and instruction pointer for a
 * newly booted CPU.
 */
extern volatile unsigned long init_rsp;
extern void (*initial_code)(void);

void __init
start_secondary(void)
{
	cpu_init();
	cpu_set(cpu_id(), cpu_online_map);

	lapic_set_timer(1000000000);
	local_irq_enable();

	cpu_idle();
}

void __init
arch_boot_cpu(unsigned int cpu)
{
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
	cpu_gdt_descr[cpu].address = (unsigned long)
	                             alloc_bootmem_aligned(PAGE_SIZE, PAGE_SIZE);

	/*
	 * Allocate memory for the new CPU's idle task.
	 */
	new_task_union = alloc_bootmem_aligned( sizeof(*new_task_union),
	                                        sizeof(*new_task_union) );
	memset(new_task_union, 0, sizeof(*new_task_union));
	new_task = &new_task_union->task_info;

	/*
	 * Initialize the bare minimum info needed to boot the new CPU.
	 */
	new_task->task_id = 0;
	new_task->aspace  = &init_aspace;
	new_task->cpu     = cpu;
	strcpy(new_task->task_name, "Idle Task");

	/*
	 * Set the initial kernel entry point and stack pointer for the new CPU.
	 */
	initial_code = start_secondary;
	init_rsp     = (unsigned long)new_task_union
	                                  + sizeof(union task_union) - 1;

	/*
	 * Boot it!
	 */
	lapic_send_init_ipi(cpu);
	lapic_send_startup_ipi(cpu, SMP_TRAMPOLINE_BASE);
	lapic_send_startup_ipi(cpu, SMP_TRAMPOLINE_BASE);
}

