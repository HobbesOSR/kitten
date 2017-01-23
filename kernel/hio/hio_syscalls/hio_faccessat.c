#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

long
hio_faccessat(int dfd, const char __user *filename, int mode)
{
	if (!syscall_isset(__NR_faccessat, current->aspace->hio_syscall_mask))
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_faccessat, 3, dfd, filename, mode);
}
