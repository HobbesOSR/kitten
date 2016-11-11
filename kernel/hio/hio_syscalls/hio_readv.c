#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern ssize_t
sys_readv(int fd, const uaddr_t iov, int iovcnt);

ssize_t
hio_readv(int	        fd,
          const uaddr_t iov,
          int           iovcnt)
{
	if ( (!syscall_isset(__NR_readv, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_readv(fd, iov, iovcnt);

	return hio_format_and_exec_syscall(__NR_readv, 3, fd, iov, iovcnt);
}
