#include <lwk/task.h>

int
sys_task_exit(
	int    status
)
{
	return task_exit(status);
}
