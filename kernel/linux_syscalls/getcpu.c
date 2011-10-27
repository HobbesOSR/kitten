#include <lwk/task.h>
#include <lwk/smp.h>
#include <arch/uaccess.h>

int
sys_getcpu(unsigned *cpu)
{
	BUG_ON(current->cpu_id != this_cpu);
	if (cpu && copy_to_user(cpu, &current->cpu_id, sizeof(current->cpu_id)))
		return -EFAULT;

	return 0;
}
