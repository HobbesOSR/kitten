#include <arch/uaccess.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

int
hio_shmctl(int shmid, int cmd, void *buf)
{
	if (!syscall_isset(__NR_shmctl, current->aspace->hio_syscall_mask))
		return -1;

	return hio_format_and_exec_syscall(__NR_shmctl, 3, shmid, cmd, buf);
}
