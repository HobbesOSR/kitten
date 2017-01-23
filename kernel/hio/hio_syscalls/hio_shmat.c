#include <arch/uaccess.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

long
hio_shmat(int shmid, char __user *shmaddr, int shmflg)
{
	if (!syscall_isset(__NR_shmat, current->aspace->hio_syscall_mask))
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_shmat, 3, shmid, shmaddr, shmflg);
}
