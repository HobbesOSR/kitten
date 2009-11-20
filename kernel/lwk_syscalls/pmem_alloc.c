#include <lwk/pmem.h>
#include <arch/uaccess.h>

int
sys_pmem_alloc(
	size_t                               size,
	size_t                               alignment,
	const struct pmem_region __user *    constraint,
	struct pmem_region __user *          result
)
{
	struct pmem_region _constraint, _result;
	int status;

	if (current->uid != 0)
		return -EPERM;

	if (copy_from_user(&_constraint, constraint, sizeof(_constraint)))
		return -EINVAL;

	if ((status = pmem_alloc(size, alignment, &_constraint, &_result)) != 0)
		return status;

	if (result && copy_to_user(result, &_result, sizeof(_result)))
		return -EINVAL;

	return 0;
}
