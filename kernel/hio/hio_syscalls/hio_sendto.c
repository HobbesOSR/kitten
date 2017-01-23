#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

long
hio_sendto(int fd, void __user *buf, size_t len, unsigned flags, struct sockaddr __user *dest_addr, int addrlen)
{
	if (!syscall_isset(__NR_sendto, current->aspace->hio_syscall_mask))
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_sendto, 6, fd, buf, len, flags, dest_addr, addrlen);
}
