#include <lwk/kernel.h>
#include <lwk/task.h>

long
sys_getgid(void)
{
	return current->gid;
}
