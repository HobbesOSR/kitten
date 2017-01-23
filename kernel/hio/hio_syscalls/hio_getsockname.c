#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

long
hio_getsockname(int fd, struct sockaddr __user *addr, int __user *addrlen)
{
	if (!syscall_isset(__NR_getsockname, current->aspace->hio_syscall_mask))
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_getsockname, 3, fd, addr, addrlen);
}
