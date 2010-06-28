#include <lwk/kernel.h>
#include <lwk/futex.h>

void
sys_exit_group(
	int	exit_status
)
{
	// Terminate all tasks in callers address space,
	// including the caller. This call does not return.
	task_exit_group((exit_status & 0xFF) << 8);
}
