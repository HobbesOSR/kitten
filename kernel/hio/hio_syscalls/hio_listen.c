#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

int
hio_listen(int sockfd,
	   int backlog)
{
	if ( (!syscall_isset(__NR_listen, current->aspace->hio_syscall_mask))
	   )
		return -ENOSYS;

	return hio_format_and_exec_syscall(__NR_listen, 2,
		sockfd, backlog);
}
