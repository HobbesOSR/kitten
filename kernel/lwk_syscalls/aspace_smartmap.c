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

	if ((src == KERNEL_ASPACE_ID) || (dst == KERNEL_ASPACE_ID))
		return -EINVAL;

	return aspace_smartmap(src, dst, start, extent);
}
