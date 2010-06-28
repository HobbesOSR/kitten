#include <lwk/task.h>
#include <arch/uaccess.h>

int
sys_getcpu(unsigned *cpu)
{
	if (cpu && copy_to_user(cpu, &current->cpu_id, sizeof(current->cpu_id)))
		return -EFAULT;

	return 0;
}
