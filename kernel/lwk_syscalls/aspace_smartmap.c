#include <lwk/aspace.h>

int
sys_aspace_smartmap(
	id_t       src,
	id_t       dst,
	vaddr_t    start,
	size_t     extent
)
{
	if (current->uid != 0)
		return -EPERM;

	if ((src < UASPACE_MIN_ID) || (src > UASPACE_MAX_ID))
		return -EINVAL;

	if ((dst < UASPACE_MIN_ID) || (dst > UASPACE_MAX_ID))
		return -EINVAL;

	return aspace_smartmap(src, dst, start, extent);
}
