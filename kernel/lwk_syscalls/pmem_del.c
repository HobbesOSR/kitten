#include <lwk/pmem.h>
#include <arch/uaccess.h>

int
sys_pmem_del(
	const struct pmem_region __user *    rgn
)
{
	struct pmem_region _rgn;

	if (current->uid != 0)
		return -EPERM;

	if (copy_from_user(&_rgn, rgn, sizeof(_rgn)))
		return -EINVAL;

	return pmem_del(&_rgn);
}
