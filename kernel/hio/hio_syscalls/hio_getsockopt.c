#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

int
hio_getsockopt(int         sockfd,
	       int         level,
	       int         optname,
	       void      * optval,
	       socklen_t * optlen)
{
	if ( (!syscall_isset(__NR_getsockopt, current->aspace->hio_syscall_mask))
	   )
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_getsockopt, 5, 
		sockfd, level, optname, optval, optlen);
}
