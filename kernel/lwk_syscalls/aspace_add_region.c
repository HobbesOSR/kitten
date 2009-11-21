#include <lwk/aspace.h>
#include <arch/uaccess.h>

int
sys_aspace_add_region(
	id_t                   id,
	vaddr_t                start,
	size_t                 extent,
	vmflags_t              flags,
	vmpagesize_t           pagesz,
	const char __user *    name
)
{
	char _name[16];

	if (current->uid != 0)
		return -EPERM;

	if ((id < UASPACE_MIN_ID) || (id > UASPACE_MAX_ID))
		return -EINVAL;

	if (strncpy_from_user(_name, name, sizeof(_name)) < 0)
		return -EFAULT;
	_name[sizeof(_name) - 1] = '\0';

	return aspace_add_region(id, start, extent, flags, pagesz, _name);
}
