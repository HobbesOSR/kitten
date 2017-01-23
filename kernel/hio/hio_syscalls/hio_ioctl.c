#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern int
sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);

long
hio_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg)
{
	if ( (!syscall_isset(__NR_ioctl, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_ioctl(fd, cmd, arg);

	return hio_format_and_exec_syscall(__NR_ioctl, 3, fd, cmd, arg);
}
