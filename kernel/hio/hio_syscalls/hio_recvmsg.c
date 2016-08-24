#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

ssize_t
hio_recvmsg(int		    sockfd,
	    struct msghdr * msg,
	    int		    flags)
{
	if ( (!syscall_isset(__NR_recvmsg, current->aspace->hio_syscall_mask))
	   )
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_recvmsg, 3,
		sockfd, msg, flags);
}
