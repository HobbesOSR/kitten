#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/aspace.h>

long
sys_getpid(void)
{
	return current->aspace->id;
}
