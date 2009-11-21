#include <lwk/aspace.h>

int
sys_aspace_destroy(
	id_t    id
)
{
	if (current->uid != 0)
		return -EPERM;

	if ((id < UASPACE_MIN_ID) || (id > UASPACE_MAX_ID))
		return -EINVAL;

	return aspace_destroy(id);
}
