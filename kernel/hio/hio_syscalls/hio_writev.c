#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern ssize_t
sys_writev(int fd, const uaddr_t iov, int iovcnt);

ssize_t
hio_writev(int           fd,
           const uaddr_t iov,
           int           iovcnt)
{
	if ( (!syscall_isset(__NR_writev, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_writev(fd, iov, iovcnt);

	return hio_format_and_exec_syscall(__NR_writev, 3, fd, iov, iovcnt);
}
