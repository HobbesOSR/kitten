#include <lwk/aspace.h>

int
sys_aspace_unsmartmap(
	id_t    src,
	id_t    dst
)
{
	if (current->uid != 0)
		return -EPERM;

	if ((src == KERNEL_ASPACE_ID) || (dst == KERNEL_ASPACE_ID))
		return -EINVAL;

	return aspace_unsmartmap(src, dst);
}
