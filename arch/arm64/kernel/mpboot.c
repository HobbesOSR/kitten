#include <lwk/kernel.h>
#include <lwk/init.h>
#include <lwk/cpuinfo.h>
#include <lwk/smp.h>
#include <lwk/delay.h>
#include <lwk/bootmem.h>
#include <lwk/task.h>
#include <lwk/sched.h>
#include <arch/atomic.h>
#include <arch/cpu_ops.h>



/*
 * as from 2.5, kernels no longer have an init_tasks structure
 * so we need some other way of telling a new secondary core
 * where to place its SVC stack
 */
struct secondary_data secondary_data;



/*
 * This is the secondary CPU boot entry.  We're using this CPUs
 * idle thread stack, but a set of temporary page tables.
 */
asmlinkage void secondary_start_kernel(void)
{

	early_printk("Hello from secondary startup\n");



	cpu_init();


	printk("Cpu initialized\n");
	printk("this_cpu = %d\n", this_cpu);

	sched_init_runqueue(this_cpu);
	core_timer_init(this_cpu);
	cpu_set(this_cpu, cpu_online_map);

	/*
	 * Enable external interrupts.
	 */
	local_irq_enable();

	schedule(); /* runs idle_task, since that's the only task
	             * on the CPU's run queue at this point */
}


#if 0
static DECLARE_COMPLETION(cpu_running);

int __cpu_up(unsigned int cpu, struct task_struct *idle)
{
	int ret;
	long status;

	
	/*
	 * Now bring the CPU into our world.
	 */
	ret = boot_secondary(cpu, idle);
	if (ret == 0) {
		/*
		 * CPU was successfully started, wait for it to come online or
		 * time out.
		 */
		wait_for_completion_timeout(&cpu_running,
					    msecs_to_jiffies(1000));

		if (!cpu_online(cpu)) {
			pr_crit("CPU%u: failed to come online\n", cpu);
			ret = -EIO;
		}
	} else {
		pr_err("CPU%u: failed to boot: %d\n", cpu, ret);
	}

	secondary_data.task = NULL;
	secondary_data.stack = NULL;
	status = READ_ONCE(secondary_data.status);
	if (ret && status) {

		if (status == CPU_MMU_OFF)
			status = READ_ONCE(__early_cpu_boot_status);

		switch (status) {
		default:
			pr_err("CPU%u: failed in unknown state : 0x%lx\n",
					cpu, status);
			break;
		case CPU_KILL_ME:
			if (!op_cpu_kill(cpu)) {
				pr_crit("CPU%u: died during early boot\n", cpu);
				break;
			}
			/* Fall through */
			pr_crit("CPU%u: may not have shut down cleanly\n", cpu);
		case CPU_STUCK_IN_KERNEL:
			pr_crit("CPU%u: is stuck in kernel\n", cpu);
			cpus_stuck_in_kernel++;
			break;
		case CPU_PANIC_KERNEL:
			panic("CPU%u detected unsupported configuration\n", cpu);
		}
	}

	return ret;
}

#endif
void __init
arch_boot_cpu(unsigned int cpu)
{
	union task_union *new_task_union;
	struct task_struct *new_task;

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
	list_head_init(&new_task->rr.sched_link);
	list_head_init(&new_task->migrate_link);


	/*
	 * We need to tell the secondary core where to find its stack and the
	 * page tables.
	 */
	secondary_data.task = new_task;
	secondary_data.stack = (unsigned long)new_task_union
	                                  + sizeof(union task_union) - 1;
	__flush_dcache_area(&secondary_data, sizeof(secondary_data));


	/*
	 * Boot it!
	 */

	cpu_ops[cpu]->cpu_boot(cpu);
}





