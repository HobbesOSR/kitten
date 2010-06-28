#include <lwk/task.h>

// Returns the callers thread ID
long
sys_gettid(void)
{
	return current->id;
}
