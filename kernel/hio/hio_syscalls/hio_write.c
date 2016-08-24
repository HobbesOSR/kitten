#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern ssize_t
sys_write(int, uaddr_t, size_t);


ssize_t
hio_write(int	  fd,
	  uaddr_t buf,
	  size_t  len)
{
	if ( (!syscall_isset(__NR_write, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_write(fd, buf, len);

	return hio_format_and_exec_syscall(__NR_write, 3, fd, buf, len);
}
