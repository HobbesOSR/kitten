#include <lwk/kernel.h>
#include <lwk/task.h>
#include <lwk/aspace.h>

int
sys_getpid(void)
{
	return (int)current->aspace->id;
}
