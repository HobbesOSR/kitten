#include <lwk/aspace.h>
#include <arch/uaccess.h>

int
sys_aspace_create(
	id_t                   id_request,
	const char __user *    name,
	id_t __user *          id
)
{
	int status;
	char _name[16];
	id_t _id;

	if (current->uid != 0)
		return -EPERM;

	if ((id_request != ANY_ID) &&
	    ((id_request < UASPACE_MIN_ID) || (id_request > UASPACE_MAX_ID)))
		return -EINVAL;

	if (strncpy_from_user(_name, name, sizeof(_name)) < 0)
		return -EFAULT;
	_name[sizeof(_name) - 1] = '\0';

	if ((status = aspace_create(id_request, _name, &_id)) != 0)
		return status;

	if (id && copy_to_user(id, &_id, sizeof(_id)))
		return -EFAULT;

	return 0;
}
