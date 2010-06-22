#include <lwk/task.h>
#include <lwk/aspace.h>
#include <arch/uaccess.h>

int
sys_task_create(
	const start_state_t __user *    start_state,
	id_t __user *                   task_id
)
{
	int status;
	start_state_t _start_state;
	id_t _task_id;

	if (current->uid != 0)
		return -EPERM;

	if (copy_from_user(&_start_state, start_state, sizeof(_start_state)))
		return -EFAULT;

	if ((_start_state.aspace_id < UASPACE_MIN_ID) ||
	    (_start_state.aspace_id > UASPACE_MAX_ID))
		return -EINVAL;

	if ((status = task_create(&_start_state, &_task_id)) != 0)
		return status;

	if (task_id && copy_to_user(task_id, &_task_id, sizeof(_task_id)))
		return -EFAULT;

	return 0;
}
