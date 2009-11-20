#include <lwk/aspace.h>

int
sys_aspace_del_region(
	id_t       id,
	vaddr_t    start,
	size_t     extent
)
{
	if (current->uid != 0)
		return -EPERM;

	if (id == KERNEL_ASPACE_ID)
		return -EINVAL;

	return aspace_del_region(id, start, extent);
}
