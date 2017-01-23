#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long
sys_write(unsigned int fd, const char __user *buf, size_t count);

long
hio_write(unsigned int fd, const char __user *buf, size_t count)
{
	if ( (!syscall_isset(__NR_write, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   )
		return sys_write(fd, buf, count);

	return hio_format_and_exec_syscall(__NR_write, 3, fd, buf, count);
}
