#include <lwk/aspace.h>
#include <arch/uaccess.h>

int
sys_aspace_virt_to_phys(
	id_t                id,
	vaddr_t             vaddr,
	paddr_t __user *    paddr
)
{
	int status;
	paddr_t _paddr;

	if ((status = aspace_virt_to_phys(id, vaddr, &_paddr)) != 0)
		return status;

	if (paddr && copy_to_user(paddr, &_paddr, sizeof(_paddr)))
		return -EFAULT;

	return 0;
}
