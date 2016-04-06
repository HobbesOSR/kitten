#include <lwk/kernel.h>
#include <lwk/xcall.h>


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
    xcall_function(cpu_online_map, shutdown_cpu, NULL, 0);
}
