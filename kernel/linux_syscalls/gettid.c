#include <lwk/task.h>

// Returns the callers thread ID
int
sys_gettid(void)
{
	return (int)current->id;
}
