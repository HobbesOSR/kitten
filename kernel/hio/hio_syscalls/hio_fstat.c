#include <arch/uaccess.h>
#include <arch-generic/fcntl.h>

#include <lwk/unistd.h>
#include <lwk/kfs.h>
#include <lwk/hio.h>
#include <lwk/aspace.h>

extern long sys_fstat(unsigned int fd, struct __old_kernel_stat __user *statbuf);

long
hio_fstat(unsigned int fd, struct __old_kernel_stat __user *statbuf)
{
	if ((!syscall_isset(__NR_fstat, current->aspace->hio_syscall_mask)) ||
	     (fdTableFile(current->fdTable, fd))
	   ) {
		return sys_fstat(fd, statbuf);
	}

	return hio_format_and_exec_syscall(__NR_fstat, 2, fd, statbuf);
}
