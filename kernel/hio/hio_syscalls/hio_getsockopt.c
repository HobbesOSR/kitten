#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

long
hio_getsockopt(int fd, int level, int optname, char __user *optval, int __user *optlen)
{
	if (!syscall_isset(__NR_getsockopt, current->aspace->hio_syscall_mask))
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_getsockopt, 5, fd, level, optname, optval, optlen);
}
