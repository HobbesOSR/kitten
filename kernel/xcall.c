#include <lwk/kernel.h>
#include <lwk/smp.h>
#include <lwk/xcall.h>

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
 * NOTE: xcall_function() must be called with interrupts enabled. This is
 *       necessary to prevent deadlock when multiple CPUs issue simultaneous
 *       cross-calls.
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
	BUG_ON(irqs_disabled());
	BUG_ON(!func);

	/* Only target online CPUs */
	cpus_and(cpu_mask, cpu_mask, cpu_online_map);

	/* Make the cross call */
	return arch_xcall_function(cpu_mask, func, info, wait);
}

