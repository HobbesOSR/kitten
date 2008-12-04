#include <lwk/kernel.h>
#include <lwk/task.h>

long
sys_getuid(void)
{
	return current->uid;
}
