#include <lwk/aspace.h>
#include <arch/uaccess.h>

int
sys_aspace_find_hole(
	id_t                id,
	vaddr_t             start_hint,
	size_t              extent,
	size_t              alignment,
	vaddr_t __user *    start
)
{
	vaddr_t _start;
	int status;

	if (current->uid != 0)
		return -EPERM;

	if (id == KERNEL_ASPACE_ID)
		return -EINVAL;

	status = aspace_find_hole(id, start_hint, extent, alignment, &_start);
	if (status)
		return status;
	
	if (start && copy_to_user(start, &_start, sizeof(_start)))
		return -EFAULT;

	return 0;
}
