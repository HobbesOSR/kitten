#include <lwk/task.h>
#include <arch/uaccess.h>

int
sys_task_get_cpu(
	id_t __user *    cpu_id
)
{
	int status;
	id_t _cpu_id;

	if ((status = task_get_cpu(&_cpu_id)) != 0)
		return status;

	if (cpu_id && copy_to_user(cpu_id, &_cpu_id, sizeof(_cpu_id)))
		return -EFAULT;

	return 0;
}
