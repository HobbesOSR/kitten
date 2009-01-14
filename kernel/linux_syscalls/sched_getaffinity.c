#include <lwk/kernel.h>
#include <lwk/task.h>
#include <arch/uaccess.h>

long
sys_sched_getaffinity(
	int			task_id,
	unsigned int		cpumask_len,
	vaddr_t __user *	cpumask
)
{
	if (task_id == 0)
		task_id = current->id;

	if (task_id != current->id)
		return -EINVAL;

	if (cpumask_len < sizeof(cpumask_t))
		return -EINVAL;

	if (copy_to_user(cpumask, &current->cpumask, sizeof(cpumask_t)))
		return -EFAULT;

	return sizeof(cpumask_t);
}
