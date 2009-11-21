#include <lwk/aspace.h>

int
sys_aspace_unsmartmap(
	id_t    src,
	id_t    dst
)
{
	if (current->uid != 0)
		return -EPERM;

	if ((src < UASPACE_MIN_ID) || (src > UASPACE_MAX_ID))
		return -EINVAL;

	if ((dst < UASPACE_MIN_ID) || (dst > UASPACE_MAX_ID))
		return -EINVAL;

	return aspace_unsmartmap(src, dst);
}
