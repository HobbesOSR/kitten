#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

ssize_t
hio_sendto(int		           sockfd,
	   const void            * buf,
	   size_t		   len,
	   int			   flags,
	   const struct sockaddr * dest_addr,
	   socklen_t		   addrlen)
{
	if ( (!syscall_isset(__NR_sendto, current->aspace->hio_syscall_mask))
	   )
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_sendto, 6,
		sockfd, buf, len, flags, dest_addr, addrlen);
}
