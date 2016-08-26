#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

int
hio_connect(int		         sockfd,
   	    const struct sockaddr * addr,
	    socklen_t               addrlen)
{
	if ( (!syscall_isset(__NR_connect, current->aspace->hio_syscall_mask))
	   )
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_connect, 3,
		sockfd, addr, addrlen);
}
