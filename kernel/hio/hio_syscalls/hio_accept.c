#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

int
hio_accept(int		     fd,
	   struct sockaddr * addr,
	   socklen_t         addrlen)
{
	if ( (!syscall_isset(__NR_accept, current->aspace->hio_syscall_mask))
	   )
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_accept, 3,
		fd, addr, addrlen);
}
