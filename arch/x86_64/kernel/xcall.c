#include <lwk/kernel.h>
#include <lwk/spinlock.h>
#include <lwk/xcall.h>
#include <arch/apic.h>
#include <arch/idt_vectors.h>
#include <arch/processor.h>

/**
 * Used to pass data to and synchronize the CPUs targeted by a cross-call.
 */
struct xcall_data_struct {
	void		(*func)(void *info);
	void *		info;
	atomic_t	started;
	atomic_t	finished;
	bool		wait;
};

/**
 * Global cross-call data pointer, protected by xcall_data_lock.
 */
static struct xcall_data_struct *xcall_data;
static DEFINE_SPINLOCK(xcall_data_lock);

/**
 * x86_64 specific code for carrying out inter-CPU function calls. 
 * This function should not be called directly. Call xcall_function() instead.
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
 */
int
arch_xcall_function(
	cpumask_t	cpu_mask,
	void		(*func)(void *info),
	void *		info,
	bool		wait
)
{
	struct xcall_data_struct data;
	unsigned int num_cpus;
	unsigned int cpu;

	/* Count how many CPUs are being targeted */
	num_cpus = cpus_weight(cpu_mask);
	if (!num_cpus)
		return 0;

	/* Fill in the xcall data structure on our stack */
	data.func = func;
	data.info = info;
	atomic_set(&data.started, 0);
	if (wait)
		atomic_set(&data.finished, 0);
	data.wait = wait;

	/* Set the global xcall data pointer */
	spin_lock(&xcall_data_lock);
	xcall_data = &data;
	wmb();

	/* Send inter-processor interrupts to the target CPUs */
	for_each_cpu_mask(cpu, cpu_mask)
		lapic_send_ipi(cpu, XCALL_FUNCTION_VECTOR);

	/* Wait for initiation responses */
	while (atomic_read(&data.started) != num_cpus)
		cpu_relax();

	/* If requested, wait for completion responses */
	if (wait) {
		while (atomic_read(&data.finished) != num_cpus)
			cpu_relax();
	}
	spin_unlock(&xcall_data_lock);

	return 0;
}

/**
 * The interrupt handler for inter-CPU function calls.
 */
void
arch_xcall_function_interrupt(struct pt_regs *regs, unsigned int vector)
{
	void (*func)(void *info) = xcall_data->func;
	void *info               = xcall_data->info;
	int wait                 = xcall_data->wait;

	/* Notify initiating CPU that we've started */
	mb();
	atomic_inc(&xcall_data->started);

	/* Execute the cross-call function */
	(*func)(info);

	/* Notify initiating CPU that the cross-call function has completed */
	if (wait) {
		mb();
		atomic_inc(&xcall_data->finished);
	}
}

