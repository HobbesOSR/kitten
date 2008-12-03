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

	BUG_ON(irqs_disabled());
	BUG_ON(!func);

	/* Only target online CPUs */
	cpus_and(cpu_mask, cpu_mask, cpu_online_map);

	/* No need to xcall ourself... we'll just call func() directly */
	if ((contains_me = cpu_isset(this_cpu, cpu_mask)))
		cpu_clear(this_cpu, cpu_mask);

	/* Perform xcall to remote CPUs */
	if ((status = arch_xcall_function(cpu_mask, func, info, wait)))
		return status;

	/* Call func() on the local CPU, if it was in cpu_mask */
	if (contains_me)
		(*func)(info);

	return 0;
}

/**
 * Sends a reschedule inter-processor interrupt to the target CPU.
 * This causes the target CPU to call schedule().
 *
 * NOTE: It is safe to call this with locks held and interrupts
 *       disabled so long as the caller will drop the locks and
 *       re-enable interrupts "soon", independent of whether the
 *       target actually receives the reschedule interrupt. 
 *       Deadlock may occur if these conditions aren't met.
 */
void
xcall_reschedule(id_t cpu)
{
	arch_xcall_reschedule(cpu);
}
