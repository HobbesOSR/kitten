#include <lwk/kernel.h>
#include <lwk/futex.h>

void
sys_exit(int status)
{
	/*
	 * If there is a clear_child_tid address set, clear it and wake it.
	 * This unblocks any pthread_join() waiters.
	 */
	if (current->clear_child_tid) {
		put_user(0, current->clear_child_tid);
		sys_futex(current->clear_child_tid, FUTEX_WAKE,
		          1, NULL, NULL, 0);
	}

	task_exit(status);
}
