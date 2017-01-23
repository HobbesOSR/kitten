#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long sys_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg);

long
hio_fcntl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	if ( (!syscall_isset(__NR_fcntl, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_fcntl(fd, cmd, arg);

	return hio_format_and_exec_syscall(__NR_fcntl, 3, fd, cmd, arg);
}
