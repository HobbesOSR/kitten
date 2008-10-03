#include <lwk/kernel.h>
#include <lwk/smp.h>
#include <lwk/xcall.h>

/**
 * Lock used to only allow one CPU at a time to initiate xcalls.
 */
static DEFINE_SPINLOCK(xcall_lock);

/**
 * Carries out an inter-CPU function call. The specified function is executed
 * on all of the target CPUs that are currently online and executes in 
 * interrupt context with interrupts disabled... it must not block and should
 * be short.
 *
 * Arguments:
 *       [IN] cpu_mask: The target CPUs of the cross-call.
 *       [IN] func:     The function to execute on each target CPU.
 *       [IN] info:     Argument to pass to func().
 *       [IN] wait:     true = wait for cross-call to fully complete.
 *
 * Returns:
 *       Success: 0
 *       Failure: Error code
 *
 * NOTE: If wait=0, care must be taken to ensure that the data pointed to by
 *       the info argument remains valid until the cross-call function func()
 *       completes on all target CPUs.
 */
int
xcall_function(
	cpumask_t	cpu_mask,
	void		(*func)(void *info),
	void *		info,
	bool		wait
)
{
	bool contains_me;
	int status;

	BUG_ON(irqs_enabled());
	BUG_ON(!func);

	/* Spin with IRQs enabled */
	while (!spin_trylock_irq(&xcall_lock))
		;
	/* IRQs disabled, no other CPUs within xcall_function() */

	/* Only target online CPUs */
	cpus_and(cpu_mask, cpu_mask, cpu_online_map);

	/* No need to xcall ourself... we'll just call func() directly */
	if ((contains_me = cpu_isset(cpu_id(), cpu_mask)))
		cpu_clear(cpu_id(), cpu_mask);

	/* Perform xcall to remote CPUs */
	if ((status = arch_xcall_function(cpu_mask, func, info, wait))) {
		spin_unlock(&xcall_lock);
		return status;
	}

	/* Call func() on the local CPU, if it was in cpu_mask */
	if (contains_me)
		(*func)(info);

	spin_unlock(&xcall_lock);
	return 0;
}
