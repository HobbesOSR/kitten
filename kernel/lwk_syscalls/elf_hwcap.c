#include <lwk/elf.h>
#include <arch/uaccess.h>

int
sys_elf_hwcap(
	id_t                 cpu_id,
	uint32_t __user *    hwcap
)
{
	int status;
	uint32_t _hwcap;
	
	if ((status = elf_hwcap(cpu_id, &_hwcap)) != 0)
		return status;

	if (hwcap && copy_to_user(hwcap, &_hwcap, sizeof(_hwcap)))
		return -EINVAL;

	return 0;
}
