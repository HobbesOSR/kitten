#include <lwk/kernel.h>
#include <lwk/params.h>
#include <lwk/string.h>
#include <lwk/log2.h>
#include <lwk/aspace.h>
#include <lwk/elf.h>
#include <lwk/auxvec.h>
#include <lwk/cpuinfo.h>
#include <arch/uaccess.h>
#include <arch/param.h>

int
elf_hwcap(id_t cpu, uint32_t *hwcap)
{
	if (!cpu_isset(cpu, cpu_online_map))
		return -ENOENT;
	*hwcap = ELF_HWCAP(cpu);
	return 0;
}

int
sys_elf_hwcap(id_t cpu, uint32_t __user *hwcap)
{
	int status;
	uint32_t _hwcap;
	
	if ((status = elf_hwcap(cpu, &_hwcap)) != 0)
		return status;

	if (hwcap && copy_to_user(hwcap, &_hwcap, sizeof(_hwcap)))
		return -EINVAL;

	return 0;
}
