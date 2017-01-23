#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

long
hio_bind(int sockfd, struct sockaddr __user *addr, int addrlen)
{
	if ((!syscall_isset(__NR_bind, current->aspace->hio_syscall_mask)))
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_bind, 3, sockfd, addr, addrlen);
}
