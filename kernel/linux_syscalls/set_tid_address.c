#include <lwk/kernel.h>
#include <lwk/task.h>

long
sys_set_tid_address(
	int __user *		child_tid_ptr
)
{
	current->clear_child_tid = child_tid_ptr;
	return current->id;
}
