#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern ssize_t
sys_read(int, char __user *, size_t);


ssize_t
hio_read(int	       fd,
	 char __user * buf,
	 size_t	       len)
{
	if ( (!syscall_isset(__NR_read, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_read(fd, buf, len);

	return hio_format_and_exec_syscall(__NR_read, 3, fd, buf, len);
}
