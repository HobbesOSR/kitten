#include <lwk/kernel.h>
#include <arch/uaccess.h>

int
sys_task_get_myid(
	id_t __user *    id
)
{
	int status;
	id_t _id;

	if ((status = task_get_myid(&_id)) != 0)
		return status;

	if (id && copy_to_user(id, &_id, sizeof(_id)))
		return -EFAULT;

	return 0;
}
