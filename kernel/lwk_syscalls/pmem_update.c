#include <lwk/kernel.h>
#include <lwk/pmem.h>
#include <arch/uaccess.h>

int
sys_pmem_update(
	const struct pmem_region __user *    update
)
{
	struct pmem_region _update;
	int status;

	if (current->uid != 0)
		return -EPERM;

	if (copy_from_user(&_update, update, sizeof(_update)))
		return -EINVAL;

	/* Only allow user-space to update user memory */
	get_cpu_var(umem_only) = true;
	status = pmem_update(&_update);
	get_cpu_var(umem_only) = false;

	return status;
}
