#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

long
hio_recvmsg(int fd, struct msghdr __user *msg, unsigned flags)
{
	if (!syscall_isset(__NR_recvmsg, current->aspace->hio_syscall_mask))
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_recvmsg, 3, fd, msg, flags);
}
