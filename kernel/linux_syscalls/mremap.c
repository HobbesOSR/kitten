#include <lwk/pmem.h>
#include <arch/uaccess.h>

unsigned long
sys_mremap(
	unsigned long old_address,
	size_t old_size,
	size_t new_size,
	int flags
)
{
	return (new_size <= old_size) ? old_address : -ENOSYS;
}
