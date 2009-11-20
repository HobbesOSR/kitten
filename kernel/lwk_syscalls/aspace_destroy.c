#include <lwk/aspace.h>

int
sys_aspace_destroy(
	id_t    id
)
{
	if (current->uid != 0)
		return -EPERM;

	if (id == KERNEL_ASPACE_ID)
		return -EINVAL;

	return aspace_destroy(id);
}
