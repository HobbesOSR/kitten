#include <lwk/pmem.h>
#include <arch/uaccess.h>

int
sys_pmem_query(
	const struct pmem_region __user *    query,
	struct pmem_region __user *          result
)
{
	struct pmem_region _query, _result;
	int status;

	if (current->uid != 0)
		return -EPERM;

	if (copy_from_user(&_query, query, sizeof(_query)))
		return -EINVAL;

	if ((status = pmem_query(&_query, &_result)) != 0)
		return status;

	if (result && copy_to_user(result, &_result, sizeof(_result)))
		return -EINVAL;

	return 0;
}
