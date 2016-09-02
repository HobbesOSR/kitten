#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

ssize_t
hio_recvfrom(int sockfd, void *buf, size_t len, int flags, void *src_addr, void *addrlen)
{
	if ( !syscall_isset(__NR_recvfrom, current->aspace->hio_syscall_mask) )
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_recvfrom, 6, sockfd, buf, len, flags, src_addr, addrlen);
}
