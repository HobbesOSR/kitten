#include <lwk/kernel.h>
#include <lwk/xcall.h>
#include <lwk/smp.h>


extern void arch_shutdown_cpu(void);

static void
shutdown_cpu(void * info)
{
	arch_shutdown_cpu();
}

/*
 * Shutdown all online cpus
 */
void
shutdown_cpus(void)
{
	printk("CPU #%u: shutting down all CPUs\n", this_cpu);
	on_each_cpu(shutdown_cpu, NULL, 0);
}
