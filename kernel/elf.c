#include <lwk/elf.h>
#include <lwk/cpuinfo.h>

int
elf_hwcap(
	id_t          cpu_id,
	uint32_t *    hwcap
)
{
	if (!cpu_isset(cpu_id, cpu_online_map))
		return -ENOENT;

	*hwcap = ELF_HWCAP(cpu_id);

	return 0;
}
