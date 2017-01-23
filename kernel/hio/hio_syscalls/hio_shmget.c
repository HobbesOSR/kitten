#include <arch/uaccess.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

long
hio_shmget(key_t key, size_t size, int flag)
{
	if (!syscall_isset(__NR_shmget, current->aspace->hio_syscall_mask))
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_shmget, 3, key, size, flag);
}
