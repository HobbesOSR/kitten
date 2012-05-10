#include <lwk/smp.h>
#include <lwk/xcall.h>

/*
 * Call a function on all processors.  May be used during early boot while
 * early_boot_irqs_disabled is set.  Use local_irq_save/restore() instead
 * of local_irq_disable/enable().
 */
int on_each_cpu(void (*func) (void *info), void *info, int wait);

int on_each_cpu(void (*func) (void *info), void *info, int wait)
{
    unsigned long flags;
    int ret = 0;

    return xcall_function( cpu_online_map, func, info, wait);
}
