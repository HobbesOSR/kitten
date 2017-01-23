#include <arch/uaccess.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

long
hio_shmctl(int shmid, int cmd, struct shmid_ds __user *buf)
{
	if (!syscall_isset(__NR_shmctl, current->aspace->hio_syscall_mask))
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_shmctl, 3, shmid, cmd, buf);
}
