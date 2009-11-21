#include <lwk/kernel.h>
#include <lwk/aspace.h>

int
sys_aspace_map_pmem(
	id_t       id,
	paddr_t    pmem,
	vaddr_t    start,
	size_t     extent
)
{
	int status;

	if (current->uid != 0)
		return -EPERM;

	if ((id < UASPACE_MIN_ID) || (id > UASPACE_MAX_ID))
		return -EINVAL;

	/* Only allow user-space to map user memory */
	get_cpu_var(umem_only) = true;
	status = aspace_map_pmem(id, pmem, start, extent);
	get_cpu_var(umem_only) = false;

	return status;
}
