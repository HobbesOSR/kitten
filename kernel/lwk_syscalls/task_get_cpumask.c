#include <lwk/task.h>
#include <arch/uaccess.h>

int
sys_task_get_cpumask(
	user_cpumask_t __user *    cpumask
)
{
	int status;
	user_cpumask_t _cpumask;

	if ((status = task_get_cpumask(&_cpumask)) != 0)
		return status;

	if (cpumask && copy_to_user(cpumask, &_cpumask, sizeof(_cpumask)))
		return -EFAULT;

	return 0;
}
