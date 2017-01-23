#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long
sys_read(unsigned int fd, char __user *buf, size_t count);

extern long
hio_read(unsigned int fd, char __user *buf, size_t count)
{
	if ( (!syscall_isset(__NR_read, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_read(fd, buf, count);

	return hio_format_and_exec_syscall(__NR_read, 3, fd, buf, count);
}
