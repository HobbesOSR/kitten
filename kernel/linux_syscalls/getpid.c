#include <lwk/kernel.h>
#include <lwk/task.h>

long
sys_getpid(void)
{
	return current->id;
}
