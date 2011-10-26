#include <lwk/task.h>

int
sys_task_switch_cpus(id_t cpu_id)
{
	return task_switch_cpus(cpu_id);
}
