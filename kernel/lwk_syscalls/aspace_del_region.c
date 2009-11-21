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

	if ((id < UASPACE_MIN_ID) || (id > UASPACE_MAX_ID))
		return -EINVAL;

	return aspace_del_region(id, start, extent);
}
